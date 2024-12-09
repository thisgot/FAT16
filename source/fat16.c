#include "fat16.h"
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <err.h>

/* calculate FAT address */
uint32_t bpb_faddress(struct fat_bpb *bpb)
{
	return bpb->reserved_sect * bpb->bytes_p_sect;
}

/* calculate FAT root address
uint32_t bpb_froot_addr(struct fat_bpb *bpb)
{
	return bpb_faddress(bpb) + bpb->n_fat * bpb->sect_per_fat * bpb->bytes_p_sect;
}
*/

uint32_t bpb_froot_addr(struct fat_bpb *bpb)
{
	if (bpb->sect_per_fat32 != 0)
	{ // FAT32
		return (bpb->root_cluster - 2) * (bpb->sector_p_clust * bpb->bytes_p_sect) + bpb_fdata_addr(bpb);
	}
	else
	{ // FAT16
		return bpb_faddress(bpb) + bpb->n_fat * bpb->sect_per_fat16 * bpb->bytes_p_sect;
	}
}

/* calculate data address
uint32_t bpb_fdata_addr(struct fat_bpb *bpb)
{
	return bpb_froot_addr(bpb) + bpb->possible_rentries * 32;
}


uint32_t bpb_faddress(struct fat_bpb *bpb) {
	return bpb->reserved_sect * bpb->bytes_p_sect;
}
*/

uint32_t bpb_fdata_addr(struct fat_bpb *bpb)
{
	if (bpb->sect_per_fat32 != 0)
	{ // FAT32
		return bpb_faddress(bpb) + (bpb->n_fat * bpb->sect_per_fat32 * bpb->bytes_p_sect);
	}
	else
	{ // FAT16
		return bpb_froot_addr(bpb) + (bpb->possible_rentries * sizeof(struct fat_dir));
	}
}

/* calculate data sector count */
uint32_t bpb_fdata_sector_count(struct fat_bpb *bpb)
{
	return bpb->large_n_sects - bpb_fdata_addr(bpb) / bpb->bytes_p_sect;
}

static uint32_t bpb_fdata_sector_count_s(struct fat_bpb *bpb)
{
	return bpb->snumber_sect - bpb_fdata_addr(bpb) / bpb->bytes_p_sect;
}

/* calculate data cluster count
uint32_t bpb_fdata_cluster_count(struct fat_bpb* bpb)
{
	uint32_t sectors = bpb_fdata_sector_count_s(bpb);

	return sectors / bpb->sector_p_clust;
}
*/

uint32_t bpb_fdata_cluster_count(struct fat_bpb *bpb)
{
	uint32_t sectors;

	if (bpb->sect_per_fat32 != 0)
	{ // FAT32
		sectors = bpb->large_n_sects - (bpb->reserved_sect + bpb->n_fat * bpb->sect_per_fat32);
	}
	else
	{ // FAT16
		sectors = bpb->large_n_sects - (bpb->reserved_sect + bpb->n_fat * bpb->sect_per_fat16);
	}

	return sectors / bpb->sector_p_clust;
}

/*
 * allows reading from a specific offset and writting the data to buff
 * returns RB_ERROR if seeking or reading failed and RB_OK if success
 */
int read_bytes(FILE *fp, unsigned int offset, void *buff, unsigned int len)
{
	if (fseek(fp, offset, SEEK_SET) != 0)
	{
		error_at_line(0, errno, __FILE__, __LINE__, "warning: error when seeking to %u", offset);
		return RB_ERROR;
	}

	size_t bytes_read = fread(buff, 1, len, fp);
	if (bytes_read != len)
	{
		error_at_line(0, errno, __FILE__, __LINE__, "warning: error reading file. Expected %u, but got %zu", len, bytes_read);
		return RB_ERROR;
	}

	return RB_OK;
}

/* read fat16's bios parameter block */
void rfat(FILE *fp, struct fat_bpb *bpb)
{
	// Lê o BPB da imagem de disco
	read_bytes(fp, 0x0, bpb, sizeof(struct fat_bpb));

	// Depuração: imprime os valores lidos
	printf("Bytes por setor: %u\n", bpb->bytes_p_sect);
	printf("Sectors per cluster: %u\n", bpb->sector_p_clust);
	printf("Sectors per FAT16: %u\n", bpb->sect_per_fat16);
	printf("Sectors per FAT32: %u\n", bpb->sect_per_fat32);

	if (bpb->sect_per_fat32 != 0)
	{
		printf("FAT32 detected.\n");
	}
	else
	{
		printf("FAT16 detected.\n");
	}

	return;
}
