//list of fixed sized records in flash memory

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "storage.h"
#include "tables.h"

/* Prototypes of private funcs*/
bool table_read_file_header(table_handle_type *handle,
                            table_header_type *table_header);
bool table_write_file_header(table_handle_type *handle,
                             table_header_type *table_header);
/* End of prototypes of private funcs*/



bool table_clean(table_handle_type *handle) {
  
  bool clean_ok=false;
  
  if (handle->used_records>0) {
    table_header_type table_header;
    
    table_header.used_records=0;
    if (!table_write_file_header(handle, &table_header)) {
      ESP_LOGE(__FUNCTION__, "table_write_file_header failed");
      goto table_clean_end;
    }
    handle->used_records=0;
  }
  clean_ok=true;
table_clean_end:
  return clean_ok;
}

bool table_count(table_handle_type *handle) {

  bool read_ok=false;
  table_header_type table_header;
  
  if (!table_read_file_header(handle, &table_header)) {
    ESP_LOGE(__FUNCTION__, "table_read_file_header failed");
    goto table_count_end;
  }
  handle->used_records=table_header.used_records;
  read_ok=true;
table_count_end:
  return read_ok;
}

bool table_init (table_handle_type *handle,
                 char *path,
                 char *user_data,
                 uint16_t user_data_size,             
                 uint16_t capacity) {
 
  struct stat st; 
  bool init_ok = false;
 
  handle->path=path;
  handle->user_data=user_data;
  handle->user_data_size=user_data_size;
  handle->capacity=capacity;

  if (0!=stat(path, &st)) {
    ESP_LOGI(__FUNCTION__, "%s not found, will create...", path);

    size_t file_size = sizeof(table_header_type) +
    handle->capacity * handle->user_data_size;
 
    if (!storage_create_file(handle->path, file_size)) {
      ESP_LOGE(__FUNCTION__, "storage_create_file failed");
      goto table_init_end;
    }
    
    if (!table_clean(handle)) {
      ESP_LOGE(__FUNCTION__, "table_clean failed");
      goto table_init_end;
    }
  }
  else {    
    if (!table_count(handle)) {
      ESP_LOGE(__FUNCTION__, "table_count failed");
      goto table_init_end;
    }    
    ESP_LOGI(__FUNCTION__, "%s found, %d records used", 
    handle->path, handle->used_records);
  }
  init_ok = true;
table_init_end:  
  return init_ok;
}

bool table_append(table_handle_type *handle) {

  bool write_ok = false;
  table_header_type table_header;
  
  if (handle->used_records>=handle->capacity) {
    ESP_LOGE(__FUNCTION__, "Out of space");
    goto table_append_end;
  }
  uint16_t offset=TABLE_OFFSET_RECORDS+handle->used_records*handle->user_data_size;
  
  if (!storage_write_block_into_file(handle->path, 
                                     (char*) handle->user_data,         // buffer
                                     handle->user_data_size,            // buffer size
                                     offset
                                     )) {
    ESP_LOGE(__FUNCTION__, "storage_write_block_into_file failed");
    goto table_append_end;
  }

  ESP_LOGI(__FUNCTION__, "offset: %d, data: %.*s", offset, 
  handle->user_data_size, handle->user_data);
  table_header.used_records=handle->used_records+1;
  if (!table_write_file_header(handle, &table_header)) {
    ESP_LOGE(__FUNCTION__, "table_write_file_header failed");
    goto table_append_end;
  }
  
  handle->used_records=handle->used_records+1;
  write_ok=true;
table_append_end:
  return write_ok;
}

bool table_read_file_header(table_handle_type *handle,
                            table_header_type *table_header) {
  bool read_ok = false;
   
  if (!storage_read_block_from_file(handle->path, 
                                    (char*) table_header, 
                                    sizeof(table_header_type),
                                    TABLE_OFFSET_FILE_HEADER)) {
    ESP_LOGE(__FUNCTION__, "storage_read_block_from_file failed");
    goto table_read_file_header_end;                                            
  }
  read_ok=true;
table_read_file_header_end:
  return read_ok;
}

bool table_write_file_header(table_handle_type *handle,
                             table_header_type *table_header) {
  bool write_ok = false;
  
  if (!storage_write_block_into_file(handle->path, 
                                     (char*) table_header, 
                                     sizeof(table_header_type),
                                     TABLE_OFFSET_FILE_HEADER)) {
    ESP_LOGE(__FUNCTION__, "storage_write_block_into_file failed");
    goto table_write_file_header_end;                                            
  }
  write_ok=true;
table_write_file_header_end:
  return write_ok;
}


bool table_read_index(table_handle_type *handle,
                             uint16_t index) {
  bool read_ok = false;
  
  uint16_t offset=TABLE_OFFSET_RECORDS+index*handle->user_data_size;
  
  if (!storage_read_block_from_file(handle->path, 
                                    (char*) handle->user_data,
                                    handle->user_data_size,
                                    TABLE_OFFSET_FILE_HEADER+
                                    offset)) {
    ESP_LOGE(__FUNCTION__, "storage_read_block_from_file failed");
    goto table_read_record_index_end;                                            
  }
  read_ok=true;
table_read_record_index_end:
  return read_ok;
}

bool table_delete_index(table_handle_type *handle, uint16_t index) {
  
  bool delete_ok = false;
  table_header_type table_header;
  void *ptr = NULL;
  
  if (0==handle->used_records) {
    ESP_LOGE(__FUNCTION__, "empty table");
    goto table_delete_record_index_end;
  }
  
  if (index>=handle->used_records) {
    ESP_LOGE(__FUNCTION__, "record not available");
    goto table_delete_record_index_end;
  }
  
  if (index<handle->used_records-1) {    
    uint16_t buffer_below = handle->user_data_size*(handle->used_records-index-1);  
    uint16_t offset_below = TABLE_OFFSET_RECORDS+(index+1)*handle->user_data_size;  
    uint16_t offset_new = offset_below-handle->user_data_size;
     
    ptr=malloc(buffer_below);
   
    if (ptr == NULL) {
      ESP_LOGE(__FUNCTION__, "Could not allocate heap memory");
      goto table_delete_record_index_end;
    }
    
    if (!storage_read_block_from_file(handle->path, 
                                      (char*) ptr,
                                      buffer_below,
                                      offset_below)) {
      ESP_LOGE(__FUNCTION__, "storage_read_block_from_file failed");
      goto table_delete_record_index_end;                                            
    }

    if (!storage_write_block_into_file(handle->path, 
                                      (char*) ptr,
                                      buffer_below,
                                      offset_new)) {
      ESP_LOGE(__FUNCTION__, "storage_write_block_into_file failed");
      goto table_delete_record_index_end;                                            
    }
  }

  table_header.used_records=handle->used_records-1;
  if (!table_write_file_header(handle, &table_header)) {
    ESP_LOGE(__FUNCTION__, "table_write_file_header failed");
    goto table_delete_record_index_end;
  }
  handle->used_records=handle->used_records-1; 
  delete_ok = true;
table_delete_record_index_end:
  if (ptr != NULL) {
    free(ptr);
  }
  return delete_ok;
}


bool table_replace_index(table_handle_type *handle,
                               uint16_t index) {
  bool replace_ok=false;
  
  
  if (0==handle->used_records) {
    ESP_LOGE(__FUNCTION__, "empty table");
    goto table_replace_index_end;
  }
  
  if (index>=handle->used_records) {
    ESP_LOGE(__FUNCTION__, "record not available");
    goto table_replace_index_end;
  }
  
  uint16_t offset=TABLE_OFFSET_RECORDS+index*handle->user_data_size;
  
  if (!storage_write_block_into_file(handle->path, 
                                    handle->user_data,
                                    handle->user_data_size,
                                    offset)) {
    ESP_LOGE(__FUNCTION__, "storage_write_block_into_file failed");
    goto table_replace_index_end;                                            
  }
  replace_ok = true;
table_replace_index_end:
  
  return replace_ok;
}



bool table_insert_index(table_handle_type *handle, uint16_t index) {
  
  bool insert_ok = false;
  table_header_type table_header;
  void *ptr = NULL;
  
  if (index == handle->used_records) {
    return table_append(handle); 
  }
  if (index > handle->used_records) {
    ESP_LOGE(__FUNCTION__, "wrong index %d", index);
    goto table_insert_index_end;
  }  
  if (handle->used_records >= handle->capacity) {
    ESP_LOGE(__FUNCTION__, "Out of space");
    goto table_insert_index_end;
  }
  
  uint16_t buffer_size = handle->user_data_size*(handle->used_records-index+1);  
  uint16_t offset = TABLE_OFFSET_RECORDS+index*handle->user_data_size;  
  
   
  ptr=malloc(buffer_size);
 
  if (ptr == NULL) {
    ESP_LOGE(__FUNCTION__, "Could not allocate heap memory");
    goto table_insert_index_end;
  }
  
  if (!storage_read_block_from_file(handle->path, 
                                    (char*) ptr+handle->user_data_size,
                                    buffer_size-handle->user_data_size,
                                    offset)) {
    ESP_LOGE(__FUNCTION__, "storage_read_block_from_file failed");
    goto table_insert_index_end;                                            
  }

  memcpy(ptr, handle->user_data, handle->user_data_size);
  
  if (!storage_write_block_into_file(handle->path, 
                                    (char*) ptr,
                                    buffer_size,
                                    offset)) {
    ESP_LOGE(__FUNCTION__, "storage_write_block_into_file failed");
    goto table_insert_index_end;                                            
  }


  table_header.used_records=handle->used_records+1;
  if (!table_write_file_header(handle, &table_header)) {
    ESP_LOGE(__FUNCTION__, "table_write_file_header failed");
    goto table_insert_index_end;
  }
  handle->used_records=handle->used_records+1; 
  insert_ok = true;
table_insert_index_end:
  if (ptr != NULL) {
    free(ptr);
  }
  return insert_ok;
}
