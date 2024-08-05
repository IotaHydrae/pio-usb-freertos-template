#include <stdio.h>
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "pio_usb.h"

// Use tinyUSB header to define USB descriptors
#include "device/usbd.h"
#include "class/hid/hid_device.h"

static usb_device_t *usb_device = NULL;

tusb_desc_device_t const desc_device = {.bLength = sizeof(tusb_desc_device_t),
                                        .bDescriptorType = TUSB_DESC_DEVICE,
                                        .bcdUSB = 0x0110,
                                        .bDeviceClass = 0x00,
                                        .bDeviceSubClass = 0x00,
                                        .bDeviceProtocol = 0x00,
                                        .bMaxPacketSize0 = 64,

                                        .idVendor = 0xCafe,
                                        .idProduct = 0xef1a,
                                        .bcdDevice = 0x0100,

                                        .iManufacturer = 0x01,
                                        .iProduct = 0x02,
                                        .iSerialNumber = 0x03,

                                        .bNumConfigurations = 0x01};


enum {
  ITF_NUM_KEYBOARD,
  ITF_NUM_MOUSE,
  ITF_NUM_TOTAL,
};

enum {
  EPNUM_KEYBOARD = 0x81,
  EPNUM_MOUSE = 0x82,
};

uint8_t const desc_hid_keyboard_report[] =
{
  TUD_HID_REPORT_DESC_KEYBOARD()
};

uint8_t const desc_hid_mouse_report[] =
{
  TUD_HID_REPORT_DESC_MOUSE()
};

const uint8_t *report_desc[] = {desc_hid_keyboard_report,
                                desc_hid_mouse_report};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + 2*TUD_HID_DESC_LEN)
uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_KEYBOARD, 0, HID_ITF_PROTOCOL_KEYBOARD,
                       sizeof(desc_hid_keyboard_report), EPNUM_KEYBOARD,
                       CFG_TUD_HID_EP_BUFSIZE, 10),
    TUD_HID_DESCRIPTOR(ITF_NUM_MOUSE, 0, HID_ITF_PROTOCOL_MOUSE,
                       sizeof(desc_hid_mouse_report), EPNUM_MOUSE,
                       CFG_TUD_HID_EP_BUFSIZE, 10),
};

static_assert(sizeof(desc_device) == 18, "device desc size error");

const char *string_descriptors_base[] = {
    [0] = (const char[]){0x09, 0x04},
    [1] = "Pico PIO USB",
    [2] = "Pico PIO USB Device",
    [3] = "123456",
};
static string_descriptor_t str_desc[4];

static void init_string_desc(void) {
  for (int idx = 0; idx < 4; idx++) {
    uint8_t len = 0;
    uint16_t *wchar_str = (uint16_t *)&str_desc[idx];
    if (idx == 0) {
      wchar_str[1] = string_descriptors_base[0][0] |
                     ((uint16_t)string_descriptors_base[0][1] << 8);
      len = 1;
    } else if (idx <= 3) {
      len = strnlen(string_descriptors_base[idx], 31);
      for (int i = 0; i < len; i++) {
        wchar_str[i + 1] = string_descriptors_base[idx][i];
      }

    } else {
      len = 0;
    }

    wchar_str[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);
  }
}

static usb_descriptor_buffers_t desc = {
    .device = (uint8_t *)&desc_device,
    .config = desc_configuration,
    .hid_report = report_desc,
    .string = str_desc
};

static portTASK_FUNCTION(pio_usb_task, pvParameters)
{
    (void)pvParameters;

    static pio_usb_configuration_t config = PIO_USB_DEFAULT_CONFIG;
    init_string_desc();
    usb_device = pio_usb_device_init(&config, &desc);

    for(;;) {
        pio_usb_device_task();
    }

    vTaskDelete(NULL);
}

static portTASK_FUNCTION(usb_transfer_task, pvParameters)
{
    (void)pvParameters;

    for (;;) {
        if (usb_device != NULL) {
            hid_keyboard_report_t keyboard_report = {0};
            keyboard_report.keycode[0] = HID_KEY_A;
            endpoint_t *ep = pio_usb_get_endpoint(usb_device, 1);
            if (ep != NULL) {
                pio_usb_set_out_data(ep, (uint8_t *)&keyboard_report, sizeof(keyboard_report));
                printf("keyboard pressed!\n");
            }
        }
        sleep_ms(500);
    }

    vTaskDelete(NULL);
}

static portTASK_FUNCTION(led_task, pvParameters)
{
    (void)pvParameters;

    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);

    for (;;) {
        gpio_put(25, 1);
        sleep_ms(500);
        gpio_put(25, 0);
        sleep_ms(500);
    }

    vTaskDelete(NULL);
}

int main()
{

    set_sys_clock_khz(120000, true);
    stdio_uart_init_full(uart1, 115200, 24, 25);

    printf("hello!\n");
    sleep_ms(10);

    TaskHandle_t handle_pio_usb_task;
    TaskHandle_t handle_usb_transfer_task;
    TaskHandle_t handle_led_task;

    // create 4x tasks with different names & 2 with handles
    xTaskCreate(pio_usb_task, "pio_usb_task", 2048, NULL, 1, &handle_pio_usb_task);
    xTaskCreate(usb_transfer_task, "usb_transfer_task", 1024, NULL, 1, &handle_usb_transfer_task);
    xTaskCreate(led_task, "led_task", 256, NULL, 1, &handle_led_task);

    // Pin Tasks
    vTaskCoreAffinitySet(handle_pio_usb_task, (1 << 0));
    vTaskCoreAffinitySet(handle_usb_transfer_task, (1 << 1));
    vTaskCoreAffinitySet(handle_led_task, (1 << 1));

    vTaskStartScheduler();
    for(;;);
}