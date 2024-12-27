#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include "esp_log.h"
#include "driver/uart.h"
#include <ctype.h>
#include "storage.h"
#include "tables.h"
#define TAG "demo"

#define UART_NUM UART_NUM_0


#define STORAGE_PARTITION_NAME "storage"
#define DEMO_BASE_PATH "/spiffs"
#define DEMO_TABLE_FILENAME "/demo"
#define DEMO_MAX_FILES 3  /* 1 used only */

#define TABLE_DEMO_MAX_RECORDS 15
#define USER_DATA_SIZE 30


#define DEMO_TABLE_FULLPATH DEMO_BASE_PATH DEMO_TABLE_FILENAME


void menu_demo (table_handle_type *handle, char* user_data)
{
#define MENU_BUFFER_SIZE 64

  static char fsm = 0;
  static uint8_t buffer[MENU_BUFFER_SIZE];
  static int buffer_len = 0;
  int read_bytes;
  static char option=0;
  int index;

  if (buffer_len>=MENU_BUFFER_SIZE) {
    printf("Buffer truncated\n");
    buffer_len=0;
  }

  switch (fsm) {
    case 0:   //print help
      printf("u (used)\t\tReturn used records\n");
      printf("c (clean)\t\tClean table\n");
      printf("a (append)\t\tAppend a record\n");
      printf("i (insert)\t\tInsert a record\n");
      printf("r (replace)\t\tReplace a record\n");
      printf("d (delete)\t\tDelete a record\n");
      printf("l (list)\t\tList all records\n");
      printf("h (help)\t\tPrint this help\n");
      printf("\n");
      fsm=1;
    break;
    case 1:   //read option
      read_bytes = uart_read_bytes(UART_NUM, buffer, 1, 5 / portTICK_PERIOD_MS);
      if (read_bytes>0 && isprint(buffer[0])) {
        option=buffer[0];         
        fsm=2;
        buffer_len=0;
      }
    break;
    case 2: //parse option 
      switch (option) {
        case 'u':
          if (table_count(handle)) {
            printf("used: %d\n\n", handle->used_records);
          }
          fsm=1;
        break;
        case 'c':        
          if (!table_clean(handle)) {
            printf("clean failed\n\n");
          } 
          else {
            printf("clean ok\n\n");
          }  
          fsm=1;
        break;
        case 'a':
          fsm=3;
          printf("append: type 'text'\n");
          buffer_len=0;
        break;
        case 'i':
          printf("insert: type 'record_number[space]text'\n");
          buffer_len=0;
          fsm=6;
        break;        
        case 'r':
          fsm=5;
          printf("replace: type 'record_number[space]text'\n");
          buffer_len=0;
          break;
        case 'd':          
          printf("delete: record number?\n");
          buffer_len=0;
          fsm=4;
        break;
        case 'h':
          fsm=0;
        break;
        case 'l':
          if (!table_count(handle)) {
            printf("table_count error\n");            
          }
          else
          {
            if (handle->used_records==0) {
              printf("table empty\n");
            }
            else {
              printf("list:\n");
              for (uint16_t i=0; i<handle->used_records; i++) {
                if (!table_read_index(handle, i)) {
                  printf("table_read_record_index error index %d\n",i);
                  fsm=1;
                  break;
                }
                printf("record %d: %s\n",i,handle->user_data);
              }              
            }
          }
          printf("\n");
          fsm=1;
        break;        
        default:
          printf("unknown option\n");
          fsm=0;
        break;
      }
      option=0;
      buffer_len=0;      
    break;
    case 3: //append
      read_bytes = uart_read_bytes(UART_NUM, buffer+buffer_len, 1, 5 / portTICK_PERIOD_MS);
      if (read_bytes>0) {        
        if (isprint(buffer[buffer_len])) {
          read_bytes = uart_write_bytes(UART_NUM, buffer+buffer_len, 1);
          buffer_len++;          
        }
        else {
          buffer[buffer_len]=0;
          strcpy(handle->user_data, (char*)buffer);
          printf("\n");
          if (!table_append(handle)) {
            printf("not appended\n\n");
          }
          else {
            printf("append ok\n\n");
          }
          fsm=1;
        }
      }    
    break;
    case 4: //delete record
     read_bytes = uart_read_bytes(UART_NUM, buffer+buffer_len, 1, 5 / portTICK_PERIOD_MS);
      if (read_bytes>0) {        
        if (isprint(buffer[buffer_len])) {
          read_bytes = uart_write_bytes(UART_NUM, buffer+buffer_len, 1);
          buffer_len++;          
        }
        else {
          buffer[buffer_len]=0;
          printf("\n");
          if (1!=sscanf((char*)buffer, "%d", &index)) {
            printf("not a number\n\n");
          }
          else {                     
            if (!table_delete_index(handle, index)) {
              printf("record %d not deleted\n\n",index);
            }
            else {
              printf("record %d deleted ok\n\n",index);
            }
          }
          fsm=1;
        }        
      }
    break;

    case 5: //replace
      read_bytes = uart_read_bytes(UART_NUM, buffer+buffer_len, 1, 5 / portTICK_PERIOD_MS);
      if (read_bytes>0) {        
        if (isprint(buffer[buffer_len])) {
          read_bytes = uart_write_bytes(UART_NUM, buffer+buffer_len, 1);
          buffer_len++;          
        }
        else {
          buffer[buffer_len]=0;
          printf("\n");
                    
          if (2!=sscanf((char*)buffer, "%d %s", &index, handle->user_data)) {
            printf("parse error\n\n");
          }
          else 
          {
            if (!table_replace_index(handle, index)) {
              printf("not replaced\n\n");
            }
            else {
              printf("replace ok\n\n");
            }
          }
          fsm=1;
        }
      }
    break;
    case 6:   //insert
     read_bytes = uart_read_bytes(UART_NUM, buffer+buffer_len, 1, 5 / portTICK_PERIOD_MS);
      if (read_bytes>0) {        
        if (isprint(buffer[buffer_len])) {
          read_bytes = uart_write_bytes(UART_NUM, buffer+buffer_len, 1);
          buffer_len++;          
        }
        else {
          buffer[buffer_len]=0;
          printf("\n");
                    
          if (2!=sscanf((char*)buffer, "%d %s", &index, handle->user_data)) {
            printf("parse error\n\n");
          }
          else 
          {                 
            if (!table_insert_index(handle, index)) {
              printf("insert failed at position %d\n\n", index);
            }
            else {
              printf("insert ok at position %d\n\n", index);
            }
          }
          fsm=1;
        }
      }
    break;
    
  }
}

void app_main(void)
{
    size_t total, used;
        
    table_handle_type handle;
    char user_data[USER_DATA_SIZE];   
    
    if (ESP_OK!=storage_init( STORAGE_PARTITION_NAME, 
                              DEMO_BASE_PATH,
                              DEMO_MAX_FILES)) {
      ESP_LOGE(TAG, "storage_init failed");
      goto app_main_loop;
    }


    if (ESP_OK!=storage_partition_information(&total, &used)) {
      ESP_LOGE(TAG, "storage_partition_information failed");
      goto app_main_loop;
    }
   
    if (!table_init (&handle,
                 DEMO_TABLE_FULLPATH,
                 user_data,
                 USER_DATA_SIZE,
                 TABLE_DEMO_MAX_RECORDS)) {
      ESP_LOGE(TAG, "table_init failed");
      goto app_main_loop;                   
                   
    }
        
app_main_loop:


    uart_driver_install(UART_NUM, 1024, 0, 0, NULL, 0);    
    while (1) {
      menu_demo(&handle, user_data);
      vTaskDelay(20/portTICK_PERIOD_MS);
    }
}
