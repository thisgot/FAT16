#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "commands.h"
#include "fat16.h"
#include "support.h"

#include <errno.h>
#include <err.h>
#include <error.h>
#include <assert.h>

#include <sys/types.h>

struct fat_dir *ls(FILE *fp, struct fat_bpb *bpb)
{
	if (bpb->sect_per_fat32 != 0)
	{ // FAT32
		uint32_t cluster = bpb->root_cluster;
		uint32_t cluster_width = bpb->sector_p_clust * bpb->bytes_p_sect;
		struct fat_dir *dirs = malloc(cluster_width);
		uint32_t data_start = bpb_fdata_addr(bpb);
		read_bytes(fp, data_start + (cluster - 2) * cluster_width, dirs, cluster_width);
		return dirs;
	}
	else
	{ // FAT16
		struct fat_dir *dirs = malloc(sizeof(struct fat_dir) * bpb->possible_rentries);
		for (int i = 0; i < bpb->possible_rentries; i++)
		{
			uint32_t offset = bpb_froot_addr(bpb) + i * sizeof(struct fat_dir);
			read_bytes(fp, offset, &dirs[i], sizeof(dirs[i]));
		}
		return dirs;
	}
}

/*
 * Função de busca na pasta raíz. Codigo original do professor,
 * altamente modificado.
 *
 * Ela itera sobre todas as bpb->possible_rentries do struct fat_dir* dirs, e
 * retorna a primeira entrada com nome igual à filename.
 */
struct far_dir_searchres find_in_root(struct fat_dir *dirs, char *filename, struct fat_bpb *bpb)
{

	struct far_dir_searchres res = {.found = false};

	for (size_t i = 0; i < bpb->possible_rentries; i++)
	{

		if (dirs[i].name[0] == '\0')
			continue;

		if (memcmp((char *)dirs[i].name, filename, FAT16STR_SIZE) == 0)
		{
			res.found = true;
			res.fdir = dirs[i];
			res.idx = i;
			break;
		}
	}

	return res;
}

/*
 * Função de ls
 *
 * Ela itéra todas as bpb->possible_rentries do diretório raiz
 * e as lê via read_bytes().

struct fat_dir *ls(FILE *fp, struct fat_bpb *bpb)
{

	struct fat_dir *dirs = malloc(sizeof (struct fat_dir) * bpb->possible_rentries);

	for (int i = 0; i < bpb->possible_rentries; i++)
	{
		uint32_t offset = bpb_froot_addr(bpb) + i * sizeof(struct fat_dir);
		read_bytes(fp, offset, &dirs[i], sizeof(dirs[i]));
	}

	return dirs;
}
*/
void mv(FILE *fp, char *source, char *dest, struct fat_bpb *bpb)
{
	char source_rname[FAT16STR_SIZE_WNULL], dest_rname[FAT16STR_SIZE_WNULL];

	bool badname = cstr_to_fat16wnull(source, source_rname) || cstr_to_fat16wnull(dest, dest_rname);
	if (badname)
	{
		fprintf(stderr, "Nome de arquivo inválido.\n");
		exit(EXIT_FAILURE);
	}

	uint32_t root_address = bpb_froot_addr(bpb);
	uint32_t root_size = sizeof(struct fat_dir) * bpb->possible_rentries;
	struct fat_dir root[root_size];

	if (read_bytes(fp, root_address, &root, root_size) == RB_ERROR)
		error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "erro ao ler struct fat_dir");

	struct far_dir_searchres dir1 = find_in_root(&root[0], source_rname, bpb);
	struct far_dir_searchres dir2 = find_in_root(&root[0], dest_rname, bpb);

	if (dir2.found == true)
		error(EXIT_FAILURE, 0, "Não permitido substituir arquivo %s via mv.", dest);
	if (dir1.found == false)
		error(EXIT_FAILURE, 0, "Não foi possível encontrar o arquivo %s.", source);

	memcpy(dir1.fdir.name, dest_rname, sizeof(char) * FAT16STR_SIZE);
	uint32_t source_address = sizeof(struct fat_dir) * dir1.idx + root_address;
	(void)fseek(fp, source_address, SEEK_SET);
	(void)fwrite(&dir1.fdir, sizeof(struct fat_dir), 1, fp);

	printf("mv %s → %s.\n", source, dest);
	return;
}
void rm(FILE *fp, char *filename, struct fat_bpb *bpb)
{
	char fat16_rname[FAT16STR_SIZE_WNULL];

	if (cstr_to_fat16wnull(filename, fat16_rname))
	{
		fprintf(stderr, "Nome de arquivo inválido.\n");
		exit(EXIT_FAILURE);
	}

	uint32_t root_address = bpb_froot_addr(bpb);
	uint32_t root_size = sizeof(struct fat_dir) * bpb->possible_rentries;

	struct fat_dir root[root_size];
	if (read_bytes(fp, root_address, &root, root_size) == RB_ERROR)
	{
		error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "erro ao ler struct fat_dir");
	}

	struct far_dir_searchres dir = find_in_root(&root[0], fat16_rname, bpb);

	if (dir.found == false)
	{
		error(EXIT_FAILURE, 0, "Não foi possível encontrar o arquivo %s.", filename);
	}

	dir.fdir.name[0] = DIR_FREE_ENTRY; // Marca a entrada como livre

	uint32_t file_address = sizeof(struct fat_dir) * dir.idx + root_address;
	(void)fseek(fp, file_address, SEEK_SET);
	(void)fwrite(&dir.fdir, sizeof(struct fat_dir), 1, fp);

	uint32_t fat_address = bpb_faddress(bpb);
	uint16_t cluster_number = dir.fdir.starting_cluster;
	uint16_t null = 0x0;
	size_t count = 0;

	while (cluster_number < FAT16_EOF_LO)
	{
		uint32_t infat_cluster_address = fat_address + cluster_number * sizeof(uint16_t);
		read_bytes(fp, infat_cluster_address, &cluster_number, sizeof(uint16_t));

		(void)fseek(fp, infat_cluster_address, SEEK_SET);
		(void)fwrite(&null, sizeof(uint16_t), 1, fp);

		count++;
	}

	printf("rm %s, %li clusters apagados.\n", filename, count);
	return;
}

struct fat16_newcluster_info fat16_find_free_cluster(FILE *fp, struct fat_bpb *bpb)
{

	/* Essa implementação de FAT16 não funciona com discos grandes. */
	assert(bpb->large_n_sects == 0);

	uint16_t cluster = 0x0;
	uint32_t fat_address = bpb_faddress(bpb);
	uint32_t total_clusters = bpb_fdata_cluster_count(bpb);

	for (cluster = 0x2; cluster < total_clusters; cluster++)
	{
		uint16_t entry;
		uint32_t entry_address = fat_address + cluster * 2;

		(void)read_bytes(fp, entry_address, &entry, sizeof(uint16_t));

		if (entry == 0x0)
			return (struct fat16_newcluster_info){.cluster = cluster, .address = entry_address};
	}

	return (struct fat16_newcluster_info){0};
}

void cp(FILE *fp, char *source, char *dest, struct fat_bpb *bpb)
{
	char source_rname[FAT16STR_SIZE_WNULL], dest_rname[FAT16STR_SIZE_WNULL];

	bool badname = cstr_to_fat16wnull(source, source_rname) || cstr_to_fat16wnull(dest, dest_rname);
	if (badname)
	{
		fprintf(stderr, "Nome de arquivo inválido.\n");
		exit(EXIT_FAILURE);
	}

	uint32_t root_address = bpb_froot_addr(bpb);
	uint32_t root_size = sizeof(struct fat_dir) * bpb->possible_rentries;
	struct fat_dir root[root_size];

	if (read_bytes(fp, root_address, &root, root_size) == RB_ERROR)
		error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "erro ao ler struct fat_dir");

	struct far_dir_searchres dir1 = find_in_root(root, source_rname, bpb);
	if (!dir1.found)
		error(EXIT_FAILURE, 0, "Não foi possível encontrar o arquivo %s.", source);

	if (find_in_root(root, dest_rname, bpb).found)
		error(EXIT_FAILURE, 0, "Não permitido substituir arquivo %s via cp.", dest);

	struct fat_dir new_dir = dir1.fdir;
	memcpy(new_dir.name, dest_rname, FAT16STR_SIZE);

	bool dentry_failure = true;
	for (int i = 0; i < bpb->possible_rentries; i++)
		if (root[i].name[0] == DIR_FREE_ENTRY || root[i].name[0] == '\0')
		{
			uint32_t dest_address = sizeof(struct fat_dir) * i + root_address;
			(void)fseek(fp, dest_address, SEEK_SET);
			(void)fwrite(&new_dir, sizeof(struct fat_dir), 1, fp);
			dentry_failure = false;
			break;
		}

	if (dentry_failure)
		error_at_line(EXIT_FAILURE, ENOSPC, __FILE__, __LINE__, "Não foi possível alocar uma entrada no diretório raiz.");

	// Lógica para copiar os clusters do arquivo...
	printf("cp %s → %s, %i clusters copiados.\n", source, dest, count);
}

void cat(FILE *fp, char *filename, struct fat_bpb *bpb)
{

	/*
	 * Leitura do diretório raiz explicado em mv().
	 */

	char rname[FAT16STR_SIZE_WNULL];

	bool badname = cstr_to_fat16wnull(filename, rname);

	if (badname)
	{
		fprintf(stderr, "Nome de arquivo inválido.\n");
		exit(EXIT_FAILURE);
	}

	uint32_t root_address = bpb_froot_addr(bpb);
	uint32_t root_size = sizeof(struct fat_dir) * bpb->possible_rentries;

	struct fat_dir root[root_size];

	if (read_bytes(fp, root_address, &root, root_size) == RB_ERROR)
		error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "erro ao ler struct fat_dir");

	struct far_dir_searchres dir = find_in_root(&root[0], rname, bpb);

	if (dir.found == false)
		error(EXIT_FAILURE, 0, "Não foi possivel encontrar o %s.", filename);

	/*
	 * Descobre-se quantos bytes o arquivo tem
	 */
	size_t bytes_to_read = dir.fdir.file_size;

	/*
	 * Endereço da região de dados e da tabela de alocação.
	 */
	uint32_t data_region_start = bpb_fdata_addr(bpb);
	uint32_t fat_address = bpb_faddress(bpb);

	/*
	 * O primeiro cluster do arquivo esta guardado na struct fat_dir.
	 */
	uint16_t cluster_number = dir.fdir.starting_cluster;

	const uint32_t cluster_width = bpb->bytes_p_sect * bpb->sector_p_clust;

	while (bytes_to_read != 0)
	{

		/* read */
		{

			/* Onde em disco está o cluster atual */
			uint32_t cluster_address = (cluster_number - 2) * cluster_width + data_region_start;

			/* Devemos ler no máximo cluster_width. */
			size_t read_in_this_sector = MIN(bytes_to_read, cluster_width);

			char filedata[cluster_width];

			/* Lemos o cluster atual */
			read_bytes(fp, cluster_address, filedata, read_in_this_sector);
			printf("%.*s", (signed)read_in_this_sector, filedata);

			bytes_to_read -= read_in_this_sector;
		}

		/*
		 * Calculamos o endereço, na tabela de alocação, de onde está a entrada
		 * que diz qual é o próximo cluster.
		 */
		uint32_t next_cluster_address = fat_address + cluster_number * sizeof(uint16_t);

		/* Lemos esta entrada, assim descobrindo qual é o próximo cluster. */
		read_bytes(fp, next_cluster_address, &cluster_number, sizeof(uint16_t));
	}

	return;
}