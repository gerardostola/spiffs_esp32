#ifndef TABLES_H
#define TABLES_H


typedef struct { 
  uint16_t used_records;
} table_header_type;

typedef struct {  
  char *path;
  char *user_data;
  uint16_t user_data_size;
  uint16_t capacity;
  uint16_t used_records;
} table_handle_type;

#define TABLE_OFFSET_FILE_HEADER 0
#define TABLE_OFFSET_RECORDS TABLE_OFFSET_FILE_HEADER + sizeof(table_header_type)

bool table_init (table_handle_type *handle,
                 char *path,
                 char *user_data,
                 uint16_t user_data_size,             
                 uint16_t capacity);

bool table_append(table_handle_type *handle);
bool table_clean(table_handle_type *handle);
bool table_count(table_handle_type *handle);
bool table_read_index(table_handle_type *handle, uint16_t index);
bool table_delete_index(table_handle_type *handle, uint16_t index);
bool table_replace_index(table_handle_type *handle, uint16_t index);
bool table_insert_index(table_handle_type *handle, uint16_t index);
#endif