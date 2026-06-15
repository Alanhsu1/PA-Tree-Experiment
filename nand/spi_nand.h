#ifndef SPI_NAND_H
#define SPI_NAND_H

#include "FreeRTOS.h"

#define MT29F1G01ABAFDWB
// #define W25N01GVZEIG

#define CS_GPIO_PORT GPIO_PORT_P4
#define CS_GPIO_PIN GPIO_PIN1

#define cs_drive_low() \
{ \
    GPIO_setOutputLowOnPin(CS_GPIO_PORT, CS_GPIO_PIN); \
} \

#define cs_drive_high() \
{ \
    GPIO_setOutputHighOnPin(CS_GPIO_PORT, CS_GPIO_PIN); \
} \

#ifdef MT29F1G01ABAFDWB

#define PAGE_SIZE 2048
#define BLOCKS_PER_PLANE 1024
#define PAGES_PER_BLOCK 64
#define PAGE_NUMBER 65536

#define FEATURE_ADDRESS_BLOCK_LOCK 0xA0
#define FEATURE_ADDRESS_CONFIGURATION 0xB0
#define FEATURE_ADDRESS_STATUS 0xC0
#define FEATURE_ADDRESS_DIE_SELECT 0xD0

#define ECC_NO_ERRORS 0b000
#define ECC_1_3_CORRECTED 0b001
#define ECC_NOT_CORRECTED 0b010
#define ECC_4_6_CORRECTED 0b011
#define ECC_7_8_CORRECTED 0b101

#define MFR_ID 0x2C
#define DEVICE_ID 0x14

#define OP_IDX 0
#define OP_RESET 0xFF
#define OP_GET_FEATURE 0x0F
#define OP_SET_FEATURE 0x1F
#define OP_READ_ID 0x9F
#define OP_PAGE_READ 0x13
#define OP_READ_FROM_CACHE_X1 0x03
#define OP_READ_FROM_CACHE_X2 0x3B
#define OP_WRITE_ENABLE 0x06
#define OP_WRITE_DISABLE 0x04
#define OP_BLOCK_ERASE 0xD8
#define OP_PROGRAM_EXECUTE 0x10
#define OP_PROGRAM_LOAD_X1 0x02

#define RESET_BUFFER_LEN 1

#define GET_FEATURE_BUFFER_LEN 3
#define GET_FEATURE_ADDRESS_IDX 1
#define GET_FEATURE_DATA_IDX 2

#define SET_FEATURE_BUFFER_LEN 3
#define SET_FEATURE_ADDRESS_IDX 1
#define SET_FEATURE_DATA_IDX 2

#define READ_ID_BUFFER_LEN 4
#define READ_ID_MFR_IDX 2
#define READ_ID_DEVICE_IDX 3

#define PAGE_READ_BUFFER_LEN 4
#define PAGE_READ_DUMMY_IDX 1
#define PAGE_READ_ADDRESS_IDX_1 2
#define PAGE_READ_ADDRESS_IDX_2 3

#define READ_FROM_CACHE_X1_BUFFER_LEN 4
#define READ_FROM_CACHE_X1_ADDRESS_IDX_1 1
#define READ_FROM_CACHE_X1_ADDRESS_IDX_2 2
#define READ_FROM_CACHE_X1_DUMMY_IDX 3

#define WRITE_ENABLE_BUFFER_LEN 1

#define WRITE_DISABLE_BUFFER_LEN 1

#define BLOCK_ERASE_BUFFER_LEN 4
#define BLOCK_ERASE_ADDRESS_IDX_1 1
#define BLOCK_ERASE_ADDRESS_IDX_2 2
#define BLOCK_ERASE_ADDRESS_IDX_3 3

#define PROGRAM_EXECUTE_BUFFER_LEN 4
#define PROGRAM_EXECUTE_ADDRESS_IDX_1 1
#define PROGRAM_EXECUTE_ADDRESS_IDX_2 2
#define PROGRAM_EXECUTE_ADDRESS_IDX_3 3

#define PROGRAM_LOAD_X1_BUFFER_LEN 3
#define PROGRAM_LOAD_X1_ADDRESS_IDX_1 1
#define PROGRAM_LOAD_X1_ADDRESS_IDX_2 2

typedef union
{
    uint8_t all;
    struct
    {
        uint8_t : 1;            // bit 0
        uint8_t WP_DISABLE : 1; // bit 1
        uint8_t TB : 1;         // bit 2
        uint8_t BP0 : 1;        // bit 3
        uint8_t BP1 : 1;        // bit 4
        uint8_t BP2 : 1;        // bit 5
        uint8_t BP3 : 1;        // bit 6
        uint8_t BRWD : 1;       // bit 7
    };
} feature_register_block_lock_t;

typedef union
{
    uint8_t all;
    struct
    {
        uint8_t : 1;        // bit 0
        uint8_t CFG0 : 1;   // bit 1
        uint8_t : 1;        // bit 2
        uint8_t : 1;        // bit 3
        uint8_t ECC_EN : 1; // bit 4
        uint8_t LOT_EN : 1; // bit 5
        uint8_t CFG1 : 1;   // bit 6
        uint8_t CFG2 : 1;   // bit 7
    };
} feature_register_configuration_t;

typedef union
{
    uint8_t all;
    struct
    {
        uint8_t OIP : 1;     // bit 0
        uint8_t WEL : 1;     // bit 1
        uint8_t E_FAIL : 1;  // bit 2
        uint8_t P_FAIL : 1;  // bit 3
        uint8_t ECCS0_2 : 3; // bit 4-6
        uint8_t CRBSY : 1;   // bit 7
    };
} feature_register_status_t;

typedef union
{
    uint8_t all;
    struct
    {
        uint8_t : 1;     // bit 0
        uint8_t : 1;     // bit 1
        uint8_t : 1;     // bit 2
        uint8_t : 1;     // bit 3
        uint8_t : 1;     // bit 4
        uint8_t : 1;     // bit 5
        uint8_t DS0 : 1; // bit 6
        uint8_t : 1;     // bit 7
    };
} feature_register_die_select_t;

/* SPI NAND COMMANDS */
int reset();
int get_feature(uint8_t address, uint8_t *data_out);
int set_feature(uint8_t address, uint8_t data_in);
int read_id();
int page_read(uint16_t address);
int read_from_cache_x1(uint16_t address, uint8_t *data_out, size_t read_len);
int write_enable();
int write_disable();
int block_erase(uint16_t address);
int program_execute(uint16_t address);
int program_load_x1(uint16_t address, uint8_t *data_in, size_t write_len);

/* USER COMMANDS */
int spi_nand_init();
int clear_all_blocks();
int erase_op(uint16_t row);
int read_op(uint16_t row, uint16_t col, uint8_t *data_out, size_t read_len);
int write_op(uint16_t row, uint16_t col, uint8_t *data_in, size_t write_len);

/* UTILITIES */
int enable_ecc();
int disable_ecc();
int check_ecc();
int unlock_block();
void poll_for_oip();

#endif /* MT29F1G01ABAFDWB */

#ifdef W25N01GVZEIG

#define PAGE_SIZE 2048
#define BLOCKS_PER_PLANE 1024
#define PAGES_PER_BLOCK 64
#define PAGE_NUMBER 65536

#define ADDRESS_PROTECTION_REGISTER 0xA0
#define ADDRESS_CONFIGURATION_REGISTER 0xB0
#define ADDRESS_STATUS_REGISTER 0xC0

#define MFR_ID 0xEF
#define DEVICE_ID_1 0xAA
#define DEVICE_ID_2 0x21

#define OP_IDX 0
#define OP_RESET 0xFF
#define OP_JEDEC_ID 0x9F
#define OP_READ_STATUS_REGISTER 0x0F
#define OP_WRITE_STATUS_REGISTER 0x1F
#define OP_WRITE_ENABLE 0x06
#define OP_WRITE_DISABLE 0x04
#define OP_BLOCK_ERASE 0xD8
#define OP_LOAD_PROGRAM_DATA 0x02
#define OP_PROGRAM_EXECUTE 0x10
#define OP_PAGE_DATA_READ 0x13
#define OP_READ_DATA 0x03

#define JEDEC_ID_BUFFER_LEN 5
#define JEDEC_ID_MFR_IDX 2
#define JEDEC_ID_DEVICE_IDX1 3
#define JEDEC_ID_DEVICE_IDX2 4

#define READ_STATUS_REGISTER_BUFFER_LEN 3
#define READ_STATUS_REGISTER_ADDRESS_IDX 1
#define READ_STATUS_REGISTER_DATA_IDX 2

#define WRITE_STATUS_REGISTER_BUFFER_LEN 3
#define WRITE_STATUS_REGISTER_ADDRESS_IDX 1
#define WRITE_STATUS_REGISTER_DATA_IDX 2

#define WRITE_ENABLE_BUFFER_LEN 1

#define WRITE_DISABLE_BUFFER_LEN 1

#define BLOCK_ERASE_BUFFER_LEN 4
#define BLOCK_ERASE_DUMMY_IDX 1
#define BLOCK_ERASE_ADDRESS_IDX_1 2
#define BLOCK_ERASE_ADDRESS_IDX_2 3

#define LOAD_PROGRAM_DATA_BUFFER_LEN 3
#define LOAD_PROGRAM_DATA_ADDRESS_IDX_1 1
#define LOAD_PROGRAM_DATA_ADDRESS_IDX_2 2

#define PROGRAM_EXECUTE_BUFFER_LEN 4
#define PROGRAM_EXECUTE_DUMMY_IDX 1
#define PROGRAM_EXECUTE_ADDRESS_IDX_1 2
#define PROGRAM_EXECUTE_ADDRESS_IDX_2 3

#define PAGE_DATA_READ_BUFFER_LEN 4
#define PAGE_DATA_READ_DUMMY_IDX 1
#define PAGE_DATA_READ_ADDRESS_IDX_1 2
#define PAGE_DATA_READ_ADDRESS_IDX_2 3

#define READ_DATA_BUFFER_LEN 4
#define READ_DATA_ADDRESS_IDX_1 1
#define READ_DATA_ADDRESS_IDX_2 2
#define READ_DATA_DUMMY_IDX 3

typedef union
{
    uint8_t all;
    struct
    {
        uint8_t SRP1 : 1; // bit 0
        uint8_t WP_E : 1; // bit 1
        uint8_t TB : 1;   // bit 2
        uint8_t BP0 : 1;  // bit 3
        uint8_t BP1 : 1;  // bit 4
        uint8_t BP2 : 1;  // bit 5
        uint8_t BP3 : 1;  // bit 6
        uint8_t SPR0 : 1; // bit 7
    };
} protection_register_t;

typedef union
{
    uint8_t all;
    struct
    {
        uint8_t : 1;       // bit 0
        uint8_t : 1;       // bit 1
        uint8_t : 1;       // bit 2
        uint8_t BUF : 1;   // bit 3
        uint8_t ECC_E : 1; // bit 4
        uint8_t SR1_L : 1; // bit 5
        uint8_t OTP_E : 1; // bit 6
        uint8_t OTP_L : 1; // bit 7
    };
} configuration_register_t;

typedef union
{
    uint8_t all;
    struct
    {
        uint8_t BUSY : 1;   // bit 0
        uint8_t WEL : 1;    // bit 1
        uint8_t E_FAIL : 1; // bit 2
        uint8_t P_FAIL : 1; // bit 3
        uint8_t ECC_0 : 1;  // bit 4
        uint8_t ECC_1 : 1;  // bit 5
        uint8_t LUT_F : 1;  // bit 6
        uint8_t : 1;        // bit 7
    };
} status_register_t;

/* SPI NAND COMMANDS */
int reset();
int jedec_id();
int read_status_register(uint8_t address, uint8_t *data_out);
int write_status_register(uint8_t address, uint8_t data_in);
int write_enable();
int write_disable();
int block_erase(uint32_t address);
int load_program_data(uint16_t address, uint8_t *data_in, size_t write_len);
int program_execute(uint32_t address);
int page_data_read(uint32_t address);
int read_data(uint16_t address, uint8_t *data_out, size_t read_len);

/* USER COMMANDS */
int spi_nand_init();
int unlock_block();
int clear_all_blocks();
int erase_op(uint32_t row);
int read_op(uint32_t row, uint16_t col, uint8_t *data_out, size_t read_len);
int write_op(uint32_t row, uint16_t col, uint8_t *data_in, size_t write_len);

/* UTILITIES */
void poll_for_busy();

#endif /* W25N01GVZEIG */

#endif /* SPI_NAND_H */
