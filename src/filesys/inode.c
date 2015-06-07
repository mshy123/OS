#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/cache.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT_BLOCK_SIZE 1
#define SINGLE_INDIRECT_BLOCK_SIZE 1
#define INDIRECT_BLOCK_NUM 128

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE(=512) bytes long. */
struct inode_disk
{  
    /* Project 4: Extensible files */
    disk_sector_t direct_sector[DIRECT_BLOCK_SIZE];                    /* Direct block sector */
    disk_sector_t single_indirect_sector[SINGLE_INDIRECT_BLOCK_SIZE];  /* Single indirect block sector */
    disk_sector_t double_indirect_sector;                              /* Double indirect block sector */
    
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    
    bool isdir;                         /* Project4 : save inode is dir or not */
    disk_sector_t parent_sec            /* Project4 : save parents directory sector */
    uint32_t unused[126 - DIRECT_BLOCK_SIZE - SINGLE_INDIRECT_BLOCK_SIZE - 1];               /* Not used. */
};

/* Project 4 : Extensible files */
struct inode_indirect_block 
{
    disk_sector_t pt[INDIRECT_BLOCK_NUM];
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Project 4 : Extensible files */
/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);

  if (pos < inode->data.length) {
    disk_sector_t result;
    int direct_sector_off = pos / DISK_SECTOR_SIZE;

    if (direct_sector_off < DIRECT_BLOCK_SIZE) {
      result = inode->data.direct_sector[direct_sector_off];
    }
    else if (direct_sector_off < DIRECT_BLOCK_SIZE + SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM) {
      int single_isector_off = (direct_sector_off - DIRECT_BLOCK_SIZE) / INDIRECT_BLOCK_NUM;
      struct inode_indirect_block *iib = malloc(sizeof(struct inode_indirect_block));
      if (iib == NULL) return -1;
      disk_read(filesys_disk, inode->data.single_indirect_sector[single_isector_off], iib);
      result = iib->pt[direct_sector_off - DIRECT_BLOCK_SIZE - single_isector_off * INDIRECT_BLOCK_NUM];
      free(iib);
    }
    else {
      int double_isector_off = (direct_sector_off - DIRECT_BLOCK_SIZE - SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM) / INDIRECT_BLOCK_NUM;
      struct inode_indirect_block *d_iib = malloc(sizeof(struct inode_indirect_block));
      if (d_iib == NULL) return -1;
      struct inode_indirect_block *s_iib = malloc(sizeof(struct inode_indirect_block));
      if (s_iib == NULL) {
        free(d_iib);
        return -1;
      }
      disk_read(filesys_disk, inode->data.double_indirect_sector, d_iib);
      disk_read(filesys_disk, d_iib->pt[double_isector_off], s_iib);
      result = s_iib->pt[direct_sector_off - DIRECT_BLOCK_SIZE - SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM - double_isector_off * INDIRECT_BLOCK_NUM];
      free(s_iib);
      free(d_iib);
    }

    return result;
  }
  else
    return -1;
}

static disk_sector_t
byte_to_sector_inode_disk (const struct inode_disk *inode, off_t pos) 
{
  ASSERT (inode != NULL);

  if (pos < inode->length) {
    disk_sector_t result;
    int direct_sector_off = pos / DISK_SECTOR_SIZE;

    if (direct_sector_off < DIRECT_BLOCK_SIZE) {
      result = inode->direct_sector[direct_sector_off];
    }
    else if (direct_sector_off < DIRECT_BLOCK_SIZE + SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM) {
      int single_isector_off = (direct_sector_off - DIRECT_BLOCK_SIZE) / INDIRECT_BLOCK_NUM;
      struct inode_indirect_block *iib = malloc(sizeof(struct inode_indirect_block));
      if (iib == NULL) return -1;
      disk_read(filesys_disk, inode->single_indirect_sector[single_isector_off], iib);
      result = iib->pt[direct_sector_off - DIRECT_BLOCK_SIZE - single_isector_off * INDIRECT_BLOCK_NUM];
      free(iib);
    }
    else {
      int double_isector_off = (direct_sector_off - DIRECT_BLOCK_SIZE - SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM) / INDIRECT_BLOCK_NUM;
      struct inode_indirect_block *d_iib = malloc(sizeof(struct inode_indirect_block));
      if (d_iib == NULL) return -1;
      struct inode_indirect_block *s_iib = malloc(sizeof(struct inode_indirect_block));
      if (s_iib == NULL) {
        free(d_iib);
        return -1;
      }
      disk_read(filesys_disk, inode->double_indirect_sector, d_iib);
      disk_read(filesys_disk, d_iib->pt[double_isector_off], s_iib);
      result = s_iib->pt[direct_sector_off - DIRECT_BLOCK_SIZE - SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM - double_isector_off * INDIRECT_BLOCK_NUM];
      free(s_iib);
      free(d_iib);
    }

    return result;
  }
  else
    return -1;
}
/* project 4 : Extensible files */
static bool
inode_single_block_expand (struct inode_disk *disk_inode, off_t size)
{
  ASSERT(disk_inode->length % DISK_SECTOR_SIZE == 0);

  disk_sector_t nsec, ssec, dsec;
  int direct_sector_off = disk_inode->length / DISK_SECTOR_SIZE;

  if (direct_sector_off < DIRECT_BLOCK_SIZE) {
    if (!free_map_allocate(1, &nsec)) return false;
    disk_inode->direct_sector[direct_sector_off] = nsec;
  }
  else if (direct_sector_off < DIRECT_BLOCK_SIZE + SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM) {
    int single_isector_off = (direct_sector_off - DIRECT_BLOCK_SIZE) / INDIRECT_BLOCK_NUM;
    struct inode_indirect_block *iib = malloc(sizeof(struct inode_indirect_block));
    if (iib == NULL) return false;
    if ((direct_sector_off - DIRECT_BLOCK_SIZE) % INDIRECT_BLOCK_NUM == 0) {
      if (!free_map_allocate(1, &ssec)) {
          free(iib);
          return false;
      }
      disk_inode->single_indirect_sector[single_isector_off] = ssec;
    }
    else {
      ssec = disk_inode->single_indirect_sector[single_isector_off];
      disk_read(filesys_disk, ssec, iib);
    }

    if (!free_map_allocate(1, &nsec)) {
        free(iib);
        return false;
    }
    iib->pt[direct_sector_off - DIRECT_BLOCK_SIZE - single_isector_off * INDIRECT_BLOCK_NUM] = nsec;
    disk_write(filesys_disk, ssec, iib);
    
    free(iib);
  }
  else {
    int double_isector_off = (direct_sector_off - DIRECT_BLOCK_SIZE - SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM) / INDIRECT_BLOCK_NUM;
    struct inode_indirect_block *d_iib = malloc(sizeof(struct inode_indirect_block));
    if (d_iib == NULL) return -1;
    struct inode_indirect_block *s_iib = malloc(sizeof(struct inode_indirect_block));
    if (s_iib == NULL) {
      free(d_iib);
      return -1;
    }
    if (direct_sector_off - DIRECT_BLOCK_SIZE - SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM == 0) {
      if (!free_map_allocate(1, &dsec)) {
          free(d_iib);
          free(s_iib);
          return false;
      }
      disk_inode->double_indirect_sector = dsec;
    }
    else {
      dsec = disk_inode->double_indirect_sector;
      disk_read(filesys_disk, dsec, d_iib);  
    }

    if ((direct_sector_off - DIRECT_BLOCK_SIZE - SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM) % INDIRECT_BLOCK_NUM == 0) {
      if(!free_map_allocate(1, &ssec)) {
          free(d_iib);
          free(s_iib);
          return false;
      }
      d_iib->pt[double_isector_off] = ssec;
    }
    else {
      ssec = d_iib->pt[double_isector_off];
      disk_read(filesys_disk, ssec, s_iib);
    }

    if (!free_map_allocate(1, &nsec)) {
        free(d_iib);
        free(s_iib);
        return false;
    }
    s_iib->pt[direct_sector_off - DIRECT_BLOCK_SIZE - SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM - double_isector_off * INDIRECT_BLOCK_NUM] = nsec;
    disk_write(filesys_disk, ssec, s_iib);
    disk_write(filesys_disk, dsec, d_iib);

    free(d_iib);
    free(s_iib);
  }

  void *buffer = malloc(size);
  if (buffer == NULL) return false;
  memset(buffer, 0x00, size);
  disk_write(filesys_disk, nsec, buffer);
  free(buffer);

  disk_inode->length += size;

  return true;
}

/* Project 4 : Extensible files */
static bool
inode_expand (struct inode_disk *disk_inode, off_t size)
{
  ASSERT (disk_inode != NULL);
  ASSERT (size >= 0);

  if (size == 0)
    return true;

  off_t curr_off = disk_inode->length % DISK_SECTOR_SIZE;
  uint32_t write_byte = 0;
  void *buffer = malloc(DISK_SECTOR_SIZE);
  disk_sector_t curr_sec;
  if (curr_off != 0) {
      write_byte = (DISK_SECTOR_SIZE - curr_off);
      if (size < write_byte) {
          write_byte = size;
      }
      curr_sec = byte_to_sector_inode_disk(disk_inode, disk_inode->length - 1);
      disk_read(filesys_disk, curr_sec, buffer);
      memset(buffer + curr_off, 0x00, write_byte);
      disk_write(filesys_disk, curr_sec, buffer);
      size -= write_byte;
      disk_inode->length += write_byte;
  }

  free(buffer);

  while (size > 0) {
      if (size >= DISK_SECTOR_SIZE) {
          size -= DISK_SECTOR_SIZE;
          if (!inode_single_block_expand(disk_inode, DISK_SECTOR_SIZE)) return false;
      }
      else {
          if (!inode_single_block_expand(disk_inode, size)) return false;
          size = 0;
      }
  }

  return true;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, bool isdir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = 0;
      disk_inode->magic = INODE_MAGIC;

      if (inode_expand (disk_inode, length)) 
      {
        disk_inode->isdir = isdir;
        disk_inode->parent_sec = ROOT_DIR_SECTOR;
        disk_write (filesys_disk, sector, disk_inode); 
        success = true;
      }

      free (disk_inode);

/*
      size_t sectors = bytes_to_sectors (length);
      if (free_map_allocate (sectors, &disk_inode->start))
        {
          disk_write (filesys_disk, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[DISK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                disk_write (filesys_disk, disk_inode->start + i, zeros); 
            }
          success = true; 
        } 
      free (disk_inode);
*/
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  disk_read (filesys_disk, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      disk_write(filesys_disk, inode->sector, &inode->data);
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
 
          off_t removed_length = inode->data.length;
          int direct_sector_off = 0;
          while ((removed_length > 0) && (direct_sector_off < DIRECT_BLOCK_SIZE))
          {
            free_cache_block(inode->data.direct_sector[direct_sector_off]);
            free_map_release(inode->data.direct_sector[direct_sector_off], 1);
            direct_sector_off++;
            removed_length -= DISK_SECTOR_SIZE;
          }

          direct_sector_off = 0;

          struct inode_indirect_block *iib = malloc(sizeof(struct inode_indirect_block));
          while ((removed_length > 0) && (direct_sector_off < SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM))
          {
            if (direct_sector_off % INDIRECT_BLOCK_NUM == 0)
            {
              disk_read(filesys_disk, inode->data.single_indirect_sector[direct_sector_off / INDIRECT_BLOCK_NUM], iib);
            }
            free_cache_block(iib->pt[direct_sector_off % INDIRECT_BLOCK_NUM]);
            free_map_release(iib->pt[direct_sector_off % INDIRECT_BLOCK_NUM], 1);
            direct_sector_off++;
            removed_length -= DISK_SECTOR_SIZE;
          }

          direct_sector_off = 0;

          struct inode_indirect_block *d_iib = malloc(sizeof(struct inode_indirect_block));
          while (removed_length > 0)
          {
            if (direct_sector_off == 0)
            {
              disk_read(filesys_disk, inode->data.double_indirect_sector, d_iib);
            }

            if (direct_sector_off % INDIRECT_BLOCK_NUM == 0)
            {
              disk_read(filesys_disk, d_iib->pt[direct_sector_off / INDIRECT_BLOCK_NUM], iib);
            }

            free_cache_block(iib->pt[direct_sector_off % INDIRECT_BLOCK_NUM]);
            free_map_release(iib->pt[direct_sector_off % INDIRECT_BLOCK_NUM], 1);
            direct_sector_off++;
            removed_length -= DISK_SECTOR_SIZE;
          } 

          free(iib);
          free(d_iib);
         
         
/*
          off_t removed_length = inode->data.length;
          int direct_sector_off = 0;
          while (removed_length > 0)
          {
            if (direct_sector_off < DIRECT_BLOCK_SIZE) {
              free_map_release (inode->data.direct_sector[direct_sector_off], 1);
              direct_sector_off++;
              removed_length -= DISK_SECTOR_SIZE;
            }
            else if (direct_sector_off < DIRECT_BLOCK_SIZE + SINGLE_INDIRECT_BLOCK_SIZE * INDIRECT_BLOCK_NUM) {
              struct inode_indirect_block *iib = malloc(sizeof(struct inode_indirect_block));

              free_map_release (inode->data.single_indirect_sector[(direct_sector_off - DIRECT_BLOCK_SIZE) / INDIRECT_BLOCK_NUM])
            }
          }
          
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
*/          
        }
      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;

	while (size > 0)
	{
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector(inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length(inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		cache_read_at(sector_idx, buffer + bytes_read, chunk_size, sector_ofs);

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}

	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
Returns the number of bytes actually written, which may be
less than SIZE if end of file is reached or an error occurs.
(Normally a write at end of file would extend the inode, but
growth is not yet implemented.) */
off_t
inode_write_at(struct inode *inode, const void *buffer_, off_t size,
off_t offset)
{
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;

	if (inode->deny_write_cnt)
		return 0;

        if (inode->data.length < size + offset) {
            inode_expand(&inode->data, (off_t)(size + offset - inode->data.length));
        }

	while (size > 0)
	{
		/* Sector to write, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector(inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length(inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE)
		{
			/* Write full sector directly to disk. */
			cache_write_at(sector_idx, buffer + bytes_written, chunk_size, 0);
		}
		else
		{
			cache_write_at(sector_idx, buffer + bytes_written, chunk_size, sector_ofs);
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;
	}

	return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool
inode_isdir (const struct inode *inode)
{
  return inode->data.isdir;
}

disk_sector_t
get_parent_sec (const struct inode *inode)
{
  return inode->data.parent_sec;
}
