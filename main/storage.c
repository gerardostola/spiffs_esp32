#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_task_wdt.h"
#include "storage.h"


#define STORAGE_BUFFER_SIZE 128

esp_vfs_spiffs_conf_t conf;

bool storage_file_exists(char *filename)
{
    bool exists = false;

    // Check if config file exists
    struct stat st;
    if (stat(filename, &st) == 0) {
        exists=true;
    }
    return exists;
}

bool storage_file_delete(char *filename)
{
    bool deleted=false;
    // Delete it if it exists
    unlink(filename);
    ESP_LOGI(__FUNCTION__, "%s deleted", filename);
    return deleted;
}

esp_err_t storage_partition_information(size_t *total, size_t *used)
{
    esp_err_t ret;
    
    *total = 0;
    *used = 0;
    ret = esp_spiffs_info(conf.partition_label, total, used);
    if (ret != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "Failed to get SPIFFS partition information (%s).", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t storage_init(char *partition_label, char *base_path, size_t max_files)
{
    size_t total, used;
    esp_err_t ret = ESP_FAIL;

    conf.base_path = base_path;
    conf.partition_label = partition_label;
    conf.max_files = max_files;
    conf.format_if_mount_failed = true;
      
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(__FUNCTION__, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(__FUNCTION__, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(__FUNCTION__, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        goto storage_init_end;
    }

    ESP_LOGI(__FUNCTION__, "Performing SPIFFS_check().");

    ret = esp_spiffs_check(conf.partition_label);
    if (ret != ESP_OK) {      
        ESP_LOGE(__FUNCTION__, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
        goto storage_init_end;
    }

    ESP_LOGI(__FUNCTION__, "SPIFFS_check() successful");    
    ret = storage_partition_information(&total, &used);    
    if (ret != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "Formatting...");        
        ret = esp_spiffs_format(conf.partition_label);
        if (ret != ESP_OK) {
          ESP_LOGE(__FUNCTION__, "Failed to format (%s)", esp_err_to_name(ret));
          goto storage_init_end;        
        }
    }
    ESP_LOGI(__FUNCTION__, "Partition size: total: %d, used: %d", total, used);
    // Check consistency of reported partition size info.
    if (used > total) {
        ESP_LOGW(__FUNCTION__, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
        ret = esp_spiffs_check(conf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK) {
            ESP_LOGE(__FUNCTION__, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            goto storage_init_end;
        }
        ESP_LOGI(__FUNCTION__, "SPIFFS_check() successful");            
    }
storage_init_end:    
    return ret;
}

bool storage_create_file(char *filename, size_t filesize) {

    bool write_ok = false;
    size_t written = 0;
    char buffer[STORAGE_BUFFER_SIZE];
        
    FILE* f = fopen(filename, "wb");
    if (NULL==f) {
      ESP_LOGE(__FUNCTION__, "Failed to create %s", filename);
      goto storage_write_binary_file_end;
    }
/*
    if (filesize!=fwrite(filedata, 1, filesize, f)) {
      ESP_LOGE(__FUNCTION__, "Failed to write %s", filename);
      fclose(f);
      goto storage_write_binary_file_end;
    }
*/



    memset(buffer, 0, STORAGE_BUFFER_SIZE);

   
    while (written < filesize) {
      size_t to_write = (filesize - written > STORAGE_BUFFER_SIZE) ? STORAGE_BUFFER_SIZE : (filesize - written);
      if (fwrite(buffer, 1, to_write, f) != to_write) {
        ESP_LOGE(__FUNCTION__, "Failed to write %s", filename);
        fclose(f);
        goto storage_write_binary_file_end;
      }
      written += to_write;
    }

    
    fflush(f);
    fsync(fileno(f));

    if (fclose(f)) {
      ESP_LOGE(__FUNCTION__, "Failed to close %s", filename);      
      goto storage_write_binary_file_end;
    }
    
    write_ok=true;
    ESP_LOGI(__FUNCTION__, "ok");
storage_write_binary_file_end:
  return write_ok;
}





bool storage_write_block_into_file(char *filename, 
                                   char *block, 
                                   size_t blocksize, 
                                   long offset) {

    bool write_ok = false;
    FILE* f = fopen(filename, "r+b");
    if (NULL==f) {
      ESP_LOGE(__FUNCTION__, "fopen %s failed", filename);
      goto storage_write_binary_block_into_file_end;
    }

    if ((off_t)-1==fseek(f, offset, SEEK_SET)) {
      ESP_LOGE(__FUNCTION__, "fseek %s failed", filename);
      goto storage_write_binary_block_into_file_end;
    }

    if (blocksize!=fwrite(block, 1, blocksize, f)) {
      ESP_LOGE(__FUNCTION__, "fwrite %s failed", filename);
      fclose(f);
      goto storage_write_binary_block_into_file_end;
    }
    fflush(f);
    fsync(fileno(f));
    if (fclose(f)) {
      ESP_LOGE(__FUNCTION__, "close %s failed", filename);      
      goto storage_write_binary_block_into_file_end;
    }  
    write_ok=true;
    ESP_LOGI(__FUNCTION__, "ok");
storage_write_binary_block_into_file_end:
  return write_ok;
}


bool storage_read_block_from_file(char *filename,
                                  char *block,  
                                  size_t blocksize, 
                                  long offset) {

    bool read_ok = false;
    FILE* f = fopen(filename, "r+b");
    if (NULL==f) {
      ESP_LOGE(__FUNCTION__, "Failed to open %s for reading", filename);
      goto storage_read_block_from_file_end;
    }

    if ((off_t)-1==fseek(f, offset, SEEK_SET)) {
      ESP_LOGE(__FUNCTION__, "fseek %s failed", filename);
      goto storage_read_block_from_file_end;
    }

    if (blocksize!=fread(block, 1, blocksize, f)) {
      ESP_LOGE(__FUNCTION__, "Failed to read %s", filename);
      fclose(f);
      goto storage_read_block_from_file_end;
    }
    if (fclose(f)) {
      ESP_LOGE(__FUNCTION__, "Failed to close %s", filename);      
      goto storage_read_block_from_file_end;
    }
    read_ok=true;
storage_read_block_from_file_end:
  return read_ok;
}


bool storage_read_file(char *filename, char *filedata, size_t filesize) {

    bool read_ok = false;
    FILE* f = fopen(filename, "r+b");
    if (NULL==f) {
      ESP_LOGE(__FUNCTION__, "Failed to open %s for reading", filename);
      goto storage_read_binary_file_end;
    }

    if (filesize!=fread(filedata, 1, filesize, f)) {
      ESP_LOGE(__FUNCTION__, "Failed to read %s", filename);
      fclose(f);
      goto storage_read_binary_file_end;
    }
    if (fclose(f)) {
      ESP_LOGE(__FUNCTION__, "Failed to close %s", filename);      
      goto storage_read_binary_file_end;
    }
    read_ok=true;
storage_read_binary_file_end:
  return read_ok;
}