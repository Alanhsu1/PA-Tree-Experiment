#include "FreeRTOS.h"
#include "driverlib.h"
#include "spi.h"
#include "spi_nand.h"
#include <debug.h>

extern Statistics stats;
extern PHASE phase;

uint16_t page_in_buffer = UINT16_MAX;

#ifdef MT29F1G01ABAFDWB

int reset()
{
    uint8_t send_buffer[RESET_BUFFER_LEN] = {0};
    
    send_buffer[OP_IDX] = OP_RESET;

    cs_drive_low();
    spi_send(send_buffer, RESET_BUFFER_LEN);
    cs_drive_high();

    poll_for_oip();

    return 0; // poll OIP ?
}

int get_feature(uint8_t address, uint8_t *data_out)
{
    uint8_t send_buffer[GET_FEATURE_BUFFER_LEN] = {0};
    uint8_t recv_buffer[GET_FEATURE_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_GET_FEATURE;
    send_buffer[GET_FEATURE_ADDRESS_IDX] = address;

    cs_drive_low();
    spi_send_recv(send_buffer, recv_buffer, GET_FEATURE_BUFFER_LEN);
    cs_drive_high();

    *data_out = recv_buffer[GET_FEATURE_DATA_IDX];

    return 0;
}

int set_feature(uint8_t address, uint8_t data_in)
{
    uint8_t send_buffer[SET_FEATURE_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_SET_FEATURE;
    send_buffer[SET_FEATURE_ADDRESS_IDX] = address;
    send_buffer[SET_FEATURE_DATA_IDX] = data_in;

    cs_drive_low();
    spi_send(send_buffer, SET_FEATURE_BUFFER_LEN);
    cs_drive_high();

    return 0;
}

int read_id()
{
    uint8_t send_buffer[READ_ID_BUFFER_LEN] = {0};
    uint8_t recv_buffer[READ_ID_BUFFER_LEN] = {0};
    
    send_buffer[OP_IDX] = OP_READ_ID;
    
    cs_drive_low();
    spi_send_recv(send_buffer, recv_buffer, READ_ID_BUFFER_LEN);
    cs_drive_high();

    if (recv_buffer[READ_ID_MFR_IDX] != MFR_ID || recv_buffer[READ_ID_DEVICE_IDX] != DEVICE_ID)
        return 1;

    return 0;
}

int page_read(uint16_t address)
{
    uint8_t send_buffer[PAGE_READ_BUFFER_LEN];

    send_buffer[OP_IDX] = OP_PAGE_READ;
    send_buffer[PAGE_READ_DUMMY_IDX] = address >> 16;
    send_buffer[PAGE_READ_ADDRESS_IDX_1] = address >> 8;
    send_buffer[PAGE_READ_ADDRESS_IDX_2] = address;

    cs_drive_low();
    spi_send(send_buffer, PAGE_READ_BUFFER_LEN);
    cs_drive_high();

    poll_for_oip();

    return check_ecc();
}

int read_from_cache_x1(uint16_t address, uint8_t *data_out, size_t read_len)
{
    uint8_t send_buffer[READ_FROM_CACHE_X1_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_READ_FROM_CACHE_X1;
    send_buffer[READ_FROM_CACHE_X1_ADDRESS_IDX_1] = address >> 8;
    send_buffer[READ_FROM_CACHE_X1_ADDRESS_IDX_2] = address;
    send_buffer[READ_FROM_CACHE_X1_DUMMY_IDX] = 0;

    cs_drive_low();
    spi_send(send_buffer, READ_FROM_CACHE_X1_BUFFER_LEN);
    spi_recv(data_out, read_len);
    cs_drive_high();

    return 0;
}

int write_enable()
{
    uint8_t send_buffer[WRITE_ENABLE_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_WRITE_ENABLE;

    cs_drive_low();
    spi_send(send_buffer, WRITE_ENABLE_BUFFER_LEN);
    cs_drive_high();

    return 0;
}

int write_disable()
{
    uint8_t send_buffer[WRITE_DISABLE_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_WRITE_DISABLE;

    cs_drive_low();
    spi_send(send_buffer, WRITE_DISABLE_BUFFER_LEN);
    cs_drive_high();

    return 0;
}

int block_erase(uint16_t address)
{
    uint8_t send_buffer[BLOCK_ERASE_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_BLOCK_ERASE;
    send_buffer[BLOCK_ERASE_ADDRESS_IDX_1] = address >> 16;
    send_buffer[BLOCK_ERASE_ADDRESS_IDX_2] = address >> 8;
    send_buffer[BLOCK_ERASE_ADDRESS_IDX_3] = address;

    cs_drive_low();
    spi_send(send_buffer, BLOCK_ERASE_BUFFER_LEN);
    cs_drive_high();

    poll_for_oip();

    return 0; // poll OIP ?
}

int program_execute(uint16_t address)
{
    uint8_t send_buffer[PROGRAM_EXECUTE_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_PROGRAM_EXECUTE;
    send_buffer[PROGRAM_EXECUTE_ADDRESS_IDX_1] = address >> 16;
    send_buffer[PROGRAM_EXECUTE_ADDRESS_IDX_2] = address >> 8;
    send_buffer[PROGRAM_EXECUTE_ADDRESS_IDX_3] = address;

    cs_drive_low();
    spi_send(send_buffer, PROGRAM_EXECUTE_BUFFER_LEN);
    cs_drive_high();

    poll_for_oip();

    return 0; // poll OIP ?
}

int program_load_x1(uint16_t address, uint8_t *data_in, size_t write_len)
{
    uint8_t send_buffer[PROGRAM_LOAD_X1_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_PROGRAM_LOAD_X1;
    send_buffer[PROGRAM_LOAD_X1_ADDRESS_IDX_1] = address >> 8;
    send_buffer[PROGRAM_LOAD_X1_ADDRESS_IDX_2] = address;

    cs_drive_low();
    spi_send(send_buffer, PROGRAM_LOAD_X1_BUFFER_LEN);
    spi_send(data_in, write_len);
    cs_drive_high();

    return 0;
}

int spi_nand_init()
{
    if (read_id() != 0)
        return 1;
    
    page_in_buffer = UINT16_MAX;
    
    unlock_block();
    // clear_all_blocks();

    // enable_ecc();
   disable_ecc();

    return 0;
}

int clear_all_blocks()
{
    write_enable();

    uint32_t i;
    for (i = 0; i < BLOCKS_PER_PLANE - 2; ++i)
        erase_op(i << 6);

    write_disable();

    return 0;
}

int erase_op(uint16_t row)
{
    write_enable();

    block_erase((row >> 6) << 6);

    write_disable();

    if (phase != PHASE_CLEAR_BLOCKS)
        ++stats.flash_erase_cnt;

    return 0;
}

int read_op(uint16_t row, uint16_t col, uint8_t *data_out, size_t read_len)
{
    int ret;
    if (page_in_buffer != row)
    {
        ++stats.flash_page_read_cnt[phase];
        ret = page_read(row);
    }
    else
    {
        ++stats.flash_buffer_hit;
        ret = 0;
    }

    page_in_buffer = row;

    if (col + read_len > PAGE_SIZE)
    {
        read_from_cache_x1(col, data_out, PAGE_SIZE - col);
        ret |= read_op(row + 1, 0, data_out + (PAGE_SIZE - col), read_len - (PAGE_SIZE - col));
    }
    else
        read_from_cache_x1(col, data_out, read_len);

    stats.flash_spi_receive_cnt[phase] += (col + read_len > PAGE_SIZE ? PAGE_SIZE - col : read_len);
    return ret;
}

int write_op(uint16_t row, uint16_t col, uint8_t *data_in, size_t write_len)
{
    write_enable();
    program_load_x1(col, data_in, write_len);
    program_execute(row);
    write_disable();

    page_in_buffer = UINT16_MAX;

    ++stats.flash_page_write_cnt[phase];
    stats.flash_spi_transmit_cnt[phase] += write_len;

    return 0;
}

int enable_ecc()
{
    feature_register_configuration_t ecc = {.all = 0};
    ecc.ECC_EN = 1;
    return set_feature(FEATURE_ADDRESS_CONFIGURATION, ecc.all);
}

int disable_ecc()
{
    feature_register_configuration_t ecc = {.all = 0};
    ecc.ECC_EN = 0;
    return set_feature(FEATURE_ADDRESS_CONFIGURATION, ecc.all);
}

int check_ecc()
{
    feature_register_status_t ecc;

    get_feature(FEATURE_ADDRESS_STATUS, &ecc.all);

    switch (ecc.ECCS0_2)
    {
    case ECC_NO_ERRORS:
    case ECC_1_3_CORRECTED:
        return 0;
    case ECC_NOT_CORRECTED:
    case ECC_4_6_CORRECTED:
    case ECC_7_8_CORRECTED:
    default:
        return 1;
    }
}

int unlock_block()
{
    feature_register_block_lock_t unlock = {.all = 0};
    return set_feature(FEATURE_ADDRESS_BLOCK_LOCK, unlock.all);
}

void poll_for_oip()
{
    feature_register_status_t status;

    while (1)
    {
        get_feature(FEATURE_ADDRESS_STATUS, &status.all);

        if (status.OIP == 0)
            return;
    }
}

#endif

#ifdef W25N01GVZEIG

int reset()
{
    return 0;
}

int jedec_id()
{
    uint8_t send_buffer[JEDEC_ID_BUFFER_LEN] = {0};
    uint8_t recv_buffer[JEDEC_ID_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_JEDEC_ID;

    cs_drive_low();
    spi_send_recv(send_buffer, recv_buffer, JEDEC_ID_BUFFER_LEN);
    cs_drive_high();

    if (recv_buffer[JEDEC_ID_MFR_IDX] != MFR_ID || recv_buffer[JEDEC_ID_DEVICE_IDX1] != DEVICE_ID_1 || recv_buffer[JEDEC_ID_DEVICE_IDX2] != DEVICE_ID_2)
        return 1;

    return 0;
}

int read_status_register(uint8_t address, uint8_t *data_out)
{
    uint8_t send_buffer[READ_STATUS_REGISTER_BUFFER_LEN] = {0};
    uint8_t recv_buffer[READ_STATUS_REGISTER_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_READ_STATUS_REGISTER;
    send_buffer[READ_STATUS_REGISTER_ADDRESS_IDX] = address;

    cs_drive_low();
    spi_send_recv(send_buffer, recv_buffer, READ_STATUS_REGISTER_BUFFER_LEN);
    cs_drive_high();

    *data_out = recv_buffer[READ_STATUS_REGISTER_DATA_IDX];

    return 0;
}

int write_status_register(uint8_t address, uint8_t data_in)
{
    uint8_t send_buffer[WRITE_STATUS_REGISTER_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_WRITE_STATUS_REGISTER;
    send_buffer[WRITE_STATUS_REGISTER_ADDRESS_IDX] = address;
    send_buffer[WRITE_STATUS_REGISTER_DATA_IDX] = data_in;

    cs_drive_low();
    spi_send(send_buffer, WRITE_STATUS_REGISTER_BUFFER_LEN);
    cs_drive_high();

    return 0;
}

int write_enable()
{
    uint8_t send_buffer[WRITE_ENABLE_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_WRITE_ENABLE;

    cs_drive_low();
    spi_send(send_buffer, WRITE_ENABLE_BUFFER_LEN);
    cs_drive_high();

    return 0;
}

int write_disable()
{
    uint8_t send_buffer[WRITE_DISABLE_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_WRITE_DISABLE;

    cs_drive_low();
    spi_send(send_buffer, WRITE_DISABLE_BUFFER_LEN);
    cs_drive_high();

    return 0;
}

int block_erase(uint32_t address)
{
    uint8_t send_buffer[BLOCK_ERASE_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_BLOCK_ERASE;
    send_buffer[BLOCK_ERASE_DUMMY_IDX] = 0;
    send_buffer[BLOCK_ERASE_ADDRESS_IDX_1] = address >> 8;
    send_buffer[BLOCK_ERASE_ADDRESS_IDX_2] = address;

    cs_drive_low();
    spi_send(send_buffer, BLOCK_ERASE_BUFFER_LEN);
    cs_drive_high();

    poll_for_busy();

    return 0;
}

int load_program_data(uint16_t address, uint8_t *data_in, size_t write_len)
{
    uint8_t send_buffer[LOAD_PROGRAM_DATA_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_LOAD_PROGRAM_DATA;
    send_buffer[LOAD_PROGRAM_DATA_ADDRESS_IDX_1] = address >> 8;
    send_buffer[LOAD_PROGRAM_DATA_ADDRESS_IDX_2] = address;

    cs_drive_low();
    spi_send(send_buffer, LOAD_PROGRAM_DATA_BUFFER_LEN);
    spi_send(data_in, write_len);
    cs_drive_high();

    return 0;
}

int program_execute(uint32_t address)
{
    uint8_t send_buffer[PROGRAM_EXECUTE_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_PROGRAM_EXECUTE;
    send_buffer[PROGRAM_EXECUTE_DUMMY_IDX] = 0;
    send_buffer[PROGRAM_EXECUTE_ADDRESS_IDX_1] = address >> 8;
    send_buffer[PROGRAM_EXECUTE_ADDRESS_IDX_2] = address;

    cs_drive_low();
    spi_send(send_buffer, PROGRAM_EXECUTE_BUFFER_LEN);
    cs_drive_high();

    poll_for_busy();

    return 0;
}

int page_data_read(uint32_t address)
{
    uint8_t send_buffer[PAGE_DATA_READ_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_PAGE_DATA_READ;
    send_buffer[PAGE_DATA_READ_DUMMY_IDX] = 0;
    send_buffer[PAGE_DATA_READ_ADDRESS_IDX_1] = address >> 8;
    send_buffer[PAGE_DATA_READ_ADDRESS_IDX_2] = address;

    cs_drive_low();
    spi_send(send_buffer, PAGE_DATA_READ_BUFFER_LEN);
    cs_drive_high();

    poll_for_busy();

    return 0;
}

int read_data(uint16_t address, uint8_t *data_out, size_t read_len)
{
    uint8_t send_buffer[READ_DATA_BUFFER_LEN] = {0};

    send_buffer[OP_IDX] = OP_READ_DATA;
    send_buffer[READ_DATA_ADDRESS_IDX_1] = address >> 8;
    send_buffer[READ_DATA_ADDRESS_IDX_2] = address;
    send_buffer[READ_DATA_DUMMY_IDX] = 0;

    cs_drive_low();
    spi_send(send_buffer, READ_DATA_BUFFER_LEN);
    spi_recv(data_out, read_len);
    cs_drive_high();

    return 0;
}

int spi_nand_init()
{
    if (jedec_id() != 0)
        return 1;

    unlock_block();
    // clear_all_blocks();

    return 0;
}

int unlock_block()
{
    protection_register_t prot = {.all = 0};
    write_status_register(ADDRESS_PROTECTION_REGISTER, prot.all);

    return 0;
}

int clear_all_blocks()
{
    uint32_t i;
    for (i = 0; i < (uint32_t)BLOCKS_PER_PLANE; ++i)
        erase_op((i << 6));

    return 0;
}

int erase_op(uint32_t row)
{
    write_enable();
    block_erase(row);

    return 0;
}

int read_op(uint32_t row, uint16_t col, uint8_t *data_out, size_t read_len)
{
    page_data_read(row);
    read_data(col, data_out, read_len);

    return 0;
}

int write_op(uint32_t row, uint16_t col, uint8_t *data_in, size_t write_len)
{
    write_enable();
    load_program_data(col, data_in, write_len);
    program_execute(row);

    return 0;
}

void poll_for_busy()
{
    status_register_t status;

    while (1)
    {
        read_status_register(ADDRESS_STATUS_REGISTER, &status.all);

        if (status.BUSY == 0)
            return;
    }
}

#endif
