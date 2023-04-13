/* generated vector source file - do not edit */
        #include "bsp_api.h"
        /* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
        #if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_MAX_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
        {
                        [0] = sci_uart_rxi_isr, /* SCI7 RXI (Received data full) */
            [1] = sci_uart_txi_isr, /* SCI7 TXI (Transmit data empty) */
            [2] = sci_uart_tei_isr, /* SCI7 TEI (Transmit end) */
            [3] = sci_uart_eri_isr, /* SCI7 ERI (Receive error) */
            [4] = glcdc_line_detect_isr, /* GLCDC LINE DETECT (Specified line) */
            [5] = drw_int_isr, /* DRW INT (Display list interrupt) */
            [6] = jpeg_jedi_isr, /* JPEG JEDI (Compression/decompression process interrupt) */
            [7] = jpeg_jdti_isr, /* JPEG JDTI (Data transfer interrupt) */
            [8] = gpt_counter_overflow_isr, /* GPT2 COUNTER OVERFLOW (Overflow) */
            [9] = spi_rxi_isr, /* SPI1 RXI (Receive buffer full) */
            [10] = spi_txi_isr, /* SPI1 TXI (Transmit buffer empty) */
            [11] = spi_tei_isr, /* SPI1 TEI (Transmission complete event) */
            [12] = spi_eri_isr, /* SPI1 ERI (Error) */
            [13] = spi_rxi_isr, /* SPI0 RXI (Receive buffer full) */
            [14] = spi_txi_isr, /* SPI0 TXI (Transmit buffer empty) */
            [15] = spi_tei_isr, /* SPI0 TEI (Transmission complete event) */
            [16] = spi_eri_isr, /* SPI0 ERI (Error) */
            [17] = iic_master_rxi_isr, /* IIC2 RXI (Receive data full) */
            [18] = iic_master_txi_isr, /* IIC2 TXI (Transmit data empty) */
            [19] = iic_master_tei_isr, /* IIC2 TEI (Transmit end) */
            [20] = iic_master_eri_isr, /* IIC2 ERI (Transfer error) */
        };
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_MAX_ENTRIES] =
        {
            [0] = BSP_PRV_IELS_ENUM(EVENT_SCI7_RXI), /* SCI7 RXI (Received data full) */
            [1] = BSP_PRV_IELS_ENUM(EVENT_SCI7_TXI), /* SCI7 TXI (Transmit data empty) */
            [2] = BSP_PRV_IELS_ENUM(EVENT_SCI7_TEI), /* SCI7 TEI (Transmit end) */
            [3] = BSP_PRV_IELS_ENUM(EVENT_SCI7_ERI), /* SCI7 ERI (Receive error) */
            [4] = BSP_PRV_IELS_ENUM(EVENT_GLCDC_LINE_DETECT), /* GLCDC LINE DETECT (Specified line) */
            [5] = BSP_PRV_IELS_ENUM(EVENT_DRW_INT), /* DRW INT (Display list interrupt) */
            [6] = BSP_PRV_IELS_ENUM(EVENT_JPEG_JEDI), /* JPEG JEDI (Compression/decompression process interrupt) */
            [7] = BSP_PRV_IELS_ENUM(EVENT_JPEG_JDTI), /* JPEG JDTI (Data transfer interrupt) */
            [8] = BSP_PRV_IELS_ENUM(EVENT_GPT2_COUNTER_OVERFLOW), /* GPT2 COUNTER OVERFLOW (Overflow) */
            [9] = BSP_PRV_IELS_ENUM(EVENT_SPI1_RXI), /* SPI1 RXI (Receive buffer full) */
            [10] = BSP_PRV_IELS_ENUM(EVENT_SPI1_TXI), /* SPI1 TXI (Transmit buffer empty) */
            [11] = BSP_PRV_IELS_ENUM(EVENT_SPI1_TEI), /* SPI1 TEI (Transmission complete event) */
            [12] = BSP_PRV_IELS_ENUM(EVENT_SPI1_ERI), /* SPI1 ERI (Error) */
            [13] = BSP_PRV_IELS_ENUM(EVENT_SPI0_RXI), /* SPI0 RXI (Receive buffer full) */
            [14] = BSP_PRV_IELS_ENUM(EVENT_SPI0_TXI), /* SPI0 TXI (Transmit buffer empty) */
            [15] = BSP_PRV_IELS_ENUM(EVENT_SPI0_TEI), /* SPI0 TEI (Transmission complete event) */
            [16] = BSP_PRV_IELS_ENUM(EVENT_SPI0_ERI), /* SPI0 ERI (Error) */
            [17] = BSP_PRV_IELS_ENUM(EVENT_IIC2_RXI), /* IIC2 RXI (Receive data full) */
            [18] = BSP_PRV_IELS_ENUM(EVENT_IIC2_TXI), /* IIC2 TXI (Transmit data empty) */
            [19] = BSP_PRV_IELS_ENUM(EVENT_IIC2_TEI), /* IIC2 TEI (Transmit end) */
            [20] = BSP_PRV_IELS_ENUM(EVENT_IIC2_ERI), /* IIC2 ERI (Transfer error) */
        };
        #endif