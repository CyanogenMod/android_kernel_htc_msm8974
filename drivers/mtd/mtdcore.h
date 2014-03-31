

extern struct mutex mtd_table_mutex;
extern struct mtd_info *__mtd_next_device(int i);

extern int add_mtd_device(struct mtd_info *mtd);
extern int del_mtd_device(struct mtd_info *mtd);
extern int add_mtd_partitions(struct mtd_info *, const struct mtd_partition *,
			      int);
extern int del_mtd_partitions(struct mtd_info *);
extern int parse_mtd_partitions(struct mtd_info *master, const char **types,
				struct mtd_partition **pparts,
				struct mtd_part_parser_data *data);

#define mtd_for_each_device(mtd)			\
	for ((mtd) = __mtd_next_device(0);		\
	     (mtd) != NULL;				\
	     (mtd) = __mtd_next_device(mtd->index + 1))
