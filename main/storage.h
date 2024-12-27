#ifndef STORAGE_INIT
#define STORAGE_INIT

#define STORAGE_VERSION 1


esp_err_t storage_init(char *partition_label, char *base_path, size_t max_files);
bool storage_create_file(char *filename, size_t filesize);
bool storage_read_binary_file(char *filename, char *filedata, size_t filesize);
esp_err_t storage_partition_information(size_t *total, size_t *used);
bool storage_write_block_into_file(char *filename, 
                                   char *block, 
                                   size_t blocksize, 
                                   long offset);
bool storage_read_block_from_file(char *filename,
                                  char *block,  
                                  size_t blocksize, 
                                  long offset);                                   

void storage_test();
#endif