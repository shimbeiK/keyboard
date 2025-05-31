/* this file was modified from hog_keyboard_demo.c */
/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 * 
 * 環境は整ったので、あとはキーパッドをキーマトリクスに置き換える
 * キーの指定方法が１６進数なので、そこを考えないとね。
 */

/*
ピンを間違えるな。3v3_OUTに接続することはないぞ！

 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"

#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "ble/gatt-service/hids_device.h"

#include "ble_hid.h"
#include "inttypes.h"

#define BACKSPACE_BUTTON 17
#define RETURN_BUTTON 18
#define row_digit 5
#define column_digit 5

#define HID_REPORT_SIZE 8
#define None 0x00

int counter = 0;
int times = 1;
int now_state = 0;
bool language_checker = 0;
bool num_checker = 0;
bool ctrl_checker[5] = {0};
bool ctrl_counter = 0;

int count_strager[2] = {11,11};
const uint rows[5] = {0, 1, 2, 3, 4};
const uint columns[5] = {5, 6, 7, 8, 9};
const uint state_rows[3] = {10, 11, 12};

//top left right downの順番
const uint joy_sticks[4] = {13, 14, 15, 16};
const uint layer_switch = 17; //プッシュスイッチ
//電源はロッカースイッチ

uint8_t button_checker[row_digit][column_digit] = { {0} };
uint8_t past_button_checker[row_digit][column_digit] = { {0} };

// from USB HID Specification 1.1, Appendix B.1
const uint8_t hid_descriptor_keyboard_boot_mode[] = {

    0x05, 0x01,                    // Usage Page (Generic Desktop)
    0x09, 0x06,                    // Usage (Keyboard)
    0xa1, 0x01,                    // Collection (Application)

    0x85,  0x01,                   // Report ID 1

    // Modifier byte

    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x08,                    //   Report Count (8)
    0x05, 0x07,                    //   Usage Page (Key codes)
    0x19, 0xe0,                    //   Usage Minimum (Keyboard LeftControl)
    0x29, 0xe7,                    //   Usage Maxium (Keyboard Right GUI)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)

    // Reserved byte

    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x08,                    //   Report Count (8)
    0x81, 0x03,                    //   Input (Constant, Variable, Absolute)

    // LED report + padding

    0x95, 0x05,                    //   Report Count (5)
    0x75, 0x01,                    //   Report Size (1)
    0x05, 0x08,                    //   Usage Page (LEDs)
    0x19, 0x01,                    //   Usage Minimum (Num Lock)
    0x29, 0x05,                    //   Usage Maxium (Kana)
    0x91, 0x02,                    //   Output (Data, Variable, Absolute)

    0x95, 0x01,                    //   Report Count (1)
    0x75, 0x03,                    //   Report Size (3)
    0x91, 0x03,                    //   Output (Constant, Variable, Absolute)

    // Keycodes

    0x95, 0x06,                    //   Report Count (6)
    0x75, 0x08,                    //   Report Size (8)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0xff,                    //   Logical Maximum (1)
    0x05, 0x07,                    //   Usage Page (Key codes)
    0x19, 0x00,                    //   Usage Minimum (Reserved (no event indicated))
    0x29, 0xff,                    //   Usage Maxium (Reserved)
    0x81, 0x00,                    //   Input (Data, Array)

    0xc0,                          // End collection
};

//
#define CHAR_ILLEGAL     0xff
#define CHAR_RETURN     '\n'
#define CHAR_ESCAPE      0x1b
#define CHAR_TAB         '\t'
#define CHAR_BACKSPACE   '\b'
#define CHAR_delete   0x4c
#define CHAR_CHANGE   0x35
#define CHAR_henkan   0x8a
#define CHAR_Mu_henkan   0x8b
#define CHAR_space   ' '

// Simplified US Keyboard with Shift modifier

//return_array_prane, return_array_num, return_array_shift, return_array_ctrl
static const uint8_t return_array_prane [4][3] = {
    {'1', '1', '1'},
    {'1', '1', '1'},
    {'1', '1', '1'},
    {'1', CHAR_ILLEGAL, CHAR_ILLEGAL}
};

// static const uint8_t return_array_number [4][3] = {
//     {'1', '2', '3'},
//     {'4','5' , '6'},
//     {'7', '8', '9'},
//     {CHAR_ILLEGAL, '0', '='}
// };

static const uint8_t return_array_shift [4][3] = {
    {'1', '1', '1'},
    {'1', '1', '1'},
    {'1', '1', '1'},
    {'1', CHAR_ILLEGAL, CHAR_ILLEGAL}
};

static const uint8_t return_array_ctrl [4][3] = {
    {'1', '1', '1'},
    {'1', '1', '1'},
    {'1', '1', '1'},
    {'1', CHAR_ILLEGAL, CHAR_ILLEGAL}
};
/**sample
 * English (US)
 */
static const uint8_t keytable_us_none [] = {
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /*   0-3 */
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',                   /*  4-13 */
    'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',                   /* 14-23 */
    'u', 'v', 'w', 'x', 'y', 'z',                                       /* 24-29 */
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',                   /* 30-39 */
    CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ',            /* 40-44 */
    '-', '=', '[', ']', '\\', CHAR_ILLEGAL, ';', '\'', 0x60, ',',       /* 45-54 */
    '.', '/', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,   /* 55-60 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 61-64 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 65-68 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 69-72 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 73-76 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 77-80 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 81-84 */
    '*', '-', '+', '\n', '1', '2', '3', '4', '5',                       /* 85-97 */
    '6', '7', '8', '9', '0', '.', 0xa7,                                 /* 97-100 */
};

static const uint8_t return_array_num [10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
static const uint8_t return_array_others [5] = {CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_delete, CHAR_TAB, CHAR_RETURN};
static const uint8_t ctrl_raws[5] = {0x01, 0x02, 0x04, 0x08, 0x00};

/*
static const uint8_t return_array_jap[10][10] = {
    { CHAR_CHANGE, 'わ', None, 'を', None, 'ん', None, None, None, None},
    {'ぉ', 'あ', 'い', 'う', 'え', 'お', 'ぁ', 'ぃ', 'ぅ', 'ぇ'},  // ぉ をあ、か、さの前に移動
    {'ご', 'か', 'き', 'く', 'け', 'こ', 'が', 'ぎ', 'ぐ', 'げ'},  // ご をか、さの前に移動
    {'ぞ', 'さ', 'し', 'す', 'せ', 'そ', 'ざ', 'じ', 'ず', 'ぜ'},  // ぞ をさの前に移動
    {'ど', 'た', 'ち', 'つ', 'て', 'と', 'だ', 'ぢ', 'づ', 'で'},  // ど をた、ち、つの前に移動
    {'の', 'な', 'に', 'ぬ', 'ね', 'の', 'っ', None, None, None},  // の をな、に、ぬの前に移動
    {'ぼ', 'は', 'ひ', 'ふ', 'へ', 'ほ', 'ば', 'び', 'ぶ', 'べ'},  // っ をは、ひ、ふの前に移動
    {'ぽ', 'ま', 'み', 'む', 'め', 'も', 'ぱ', 'ぴ', 'ぷ', 'ぺ'},  // ぱ をま、み、むの前に移動
    {None, 'や', 'ゃ', 'ゆ', 'ゅ', 'よ', 'ょ', None, CHAR_space, CHAR_TAB},
    {none, 'ら', 'り', 'る', 'れ', 'ろ', None, None, CHAR_ESCAPE, CHAR_henkan}
};
*/
static const uint8_t return_array_jap[10][10][3] = {
    {{CHAR_CHANGE, None, None}, {'w', 'a', None}, {'w', 'o', None}, {'n', 'n', None}, {',', None, None}, {'.', None, None}, {'/', None, None}, {'[', None, None}, {']', None, None}, {':', None, None}},   // 1行目
    {{'x', 'o', None}, {'a', None, None}, {'i', None, None}, {'u', None, None}, {'e', None, None}, {'o', None, None}, {'x', 'a', None}, {'x', 'i', None}, {'x', 'u', None}, {'x', 'e', None}},         // 2行目
    {{'g', 'o', None}, {'k', 'a', None}, {'k', 'i', None}, {'k', 'u', None}, {'k', 'e', None}, {'k', 'o', None}, {'g', 'a', None}, {'g', 'i', None}, {'g', 'u', None}, {'g', 'e', None}},         // 3行目
    {{'z', 'o', None}, {'s', 'a', None}, {'s', 'i', None}, {'s', 'u', None}, {'s', 'e', None}, {'s', 'o', None}, {'z', 'a', None}, {'z', 'i', None}, {'z', 'u', None}, {'z', 'e', None}},         // 4行目
    {{'d', 'o', None}, {'t', 'a', None}, {'t', 'i', None}, {'t', 'u', None}, {'t', 'e', None}, {'t', 'o', None}, {'d', 'a', None}, {'d', 'i', None}, {'d', 'u', None}, {'d', 'e', None}},         // 5行目
    {{'^', None, None}, {'n', 'a', None}, {'n', 'i', None}, {'n', 'u', None}, {'n', 'e', None}, {'n', 'o', None}, {'x', 't', 'u'}, {None, None, None}, {None, None, None}, {None, None, None}},   // 6行目
    {{'b', 'o', None}, {'h', 'a', None}, {'h', 'i', None}, {'h', 'u', None}, {'h', 'e', None}, {'h', 'o', None}, {'b', 'a', None}, {'b', 'i', None}, {'b', 'u', None}, {'b', 'e', None}},         // 7行目
    {{'p', 'o', None}, {'m', 'a', None}, {'m', 'i', None}, {'m', 'u', None}, {'m', 'e', None}, {'m', 'o', None}, {'p', 'a', None}, {'p', 'i', None}, {'p', 'u', None}, {'p', 'e', None}},         // 8行目
    {{'+', None, None}, {'y', 'a', None}, {'x', 'y', 'a'}, {'y', 'u', None}, {'x', 'y', 'u'}, {'y', 'o', None}, {'x', 'y', 'o'}, {'@', None, None}, {CHAR_space, None, None}, {CHAR_TAB, None, None}},     // 9行目
    {{'-', None, None}, {'r', 'a', None}, {'r', 'i', None}, {'r', 'u', None}, {'r', 'e', None}, {'r', 'o', None}, {'\\', None, None}, {'^', None, None}, {CHAR_ESCAPE, None, None}, {CHAR_henkan, None, None}}      // 10行目
};

// Shift割り当て
static const uint8_t return_array_jap_shift[10][10] = {
    {'1', None, None, None, None, None, None, None, None, None},
    {None, '/', '3', '[', ']', '4', None, None, None, None},
    {None, None, '-', None, None, None, None, None, None, None},
    {None, None, None, '8', None, None, None, None, None, None},
    {None, None, None, None, '9', None, None, None, None, None},
    {None, None, None, None, None, '^', None, None, None, None},
    {None, None, None, None, None, None, '5', None, None, None},
    {None, None, None, None, None, None, None, '6', None, None},
    {None, None, None, None, None, None, None, None, ',', None},
    {None, None, None, None, None, None, None, None, None, '.'},
};

// ctrl割り当て
static const uint8_t return_array_jap_ctrl[10] = 
    {'d', 'a', 'c', 'v', 's', 'y', 'z', 'x', 'f', 'h'};

static const uint8_t return_array_eng [10][10] = {
    {CHAR_CHANGE, None, None, None, None, None, None, None, None, None},
    {CHAR_Mu_henkan, 'a', 'b', 'c', 'd', 'e', None, None, None, None},
    {None, 'f', 'g', 'h', 'i', 'j', None, None, None, None},
    {None, 'k', 'l', 'm', 'n', 'o', None, None, None, None},
    {None, 'p', 'q', 'r', 's', 't', None, None, None, None},
    {None, 'u', 'v', 'w', 'x', 'y', None, None, None, None},
    {None, 'z', None, None, None, None, None, None, None, None},
    {None, None, None, None, None, None, None, None, None, None},
    {None, None, None, None, None, None, None, None, ' ', CHAR_TAB},
    {None, None, None, None, None, None, None, None, CHAR_ESCAPE, CHAR_henkan},
};

static const uint8_t keytable_us_shift[] = {
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /*  0-3  */
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',                   /*  4-13 */
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',                   /* 14-23 */
    'U', 'V', 'W', 'X', 'Y', 'Z',                                       /* 24-29 */
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',                   /* 30-39 */
    CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ',            /* 40-44 */
    '_', '+', '{', '}', '|', CHAR_ILLEGAL, ':', '"', 0x7E, '<',         /* 45-54 */
    '>', '?', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,   /* 55-60 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 61-64 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 65-68 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 69-72 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 73-76 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 77-80 */
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             /* 81-84 */
    '*', '-', '+', '\n', '1', '2', '3', '4', '5',                       /* 85-97 */
    '6', '7', '8', '9', '0', '.', 0xb1,                                 /* 97-100 */
};

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static uint8_t battery = 100;
static hci_con_handle_t con_handle = HCI_CON_HANDLE_INVALID;
static uint8_t protocol_mode = HID_PROTOCOL_MODE_REPORT;

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    // Name
    0x0d, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'H', 'I', 'D', ' ', 'K', 'e', 'y', 'b', 'o', 'a', 'r', 'd',
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 
                ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE & 0xff, ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE >> 8,
    // Appearance HID - Keyboard (Category 15, Sub-Category 1)
    0x03, BLUETOOTH_DATA_TYPE_APPEARANCE, 0xC1, 0x03,
};
const uint8_t adv_data_len = sizeof(adv_data);

// Buffer for 30 characters
static uint8_t key_input_storage[30];
static btstack_ring_buffer_t key_input_buffer;

// HID Keyboard lookup
static int lookup_keycode(uint8_t character, const uint8_t * table, int size, uint8_t * keycode){
    int i;
    for (i=0;i<size;i++){
        if (table[i] != character) continue;
        *keycode = i;
        return 1;
    }
    return 0;
}

static int keycode_and_modifer_us_for_character(uint8_t character, uint8_t * keycode, uint8_t * modifier){
    int found;
    found = lookup_keycode(character, keytable_us_none, sizeof(keytable_us_none), keycode);
    if (found) {
        *modifier = 0;  // none
        return 1;
    }
    found = lookup_keycode(character, keytable_us_shift, sizeof(keytable_us_shift), keycode);
    if (found) {
        *modifier = 2;  // shift
        return 1;
    }
    return 0;
}

// HID Report sending
static void send_report(int modifier, int keycode){
    uint8_t report[] = {  modifier, 0, keycode, 0, 0, 0, 0, 0};
    switch (protocol_mode){
        case 0:
            hids_device_send_boot_keyboard_input_report(con_handle, report, sizeof(report));
            break;
        case 1:
           hids_device_send_input_report(con_handle, report, sizeof(report));
           break;
        default:
            break;
    }
}

static enum {
    W4_INPUT,
    W4_CAN_SEND_FROM_BUFFER,
    W4_CAN_SEND_KEY_UP,
} state;


static void typing_can_send_now(void){
    switch (state){
        case W4_CAN_SEND_FROM_BUFFER:
            while (1){
                uint8_t c;
                uint32_t num_bytes_read;

                btstack_ring_buffer_read(&key_input_buffer, &c, 1, &num_bytes_read);
                if (num_bytes_read == 0){
                    state = W4_INPUT;
                    break;
                }

                uint8_t modifier;
                uint8_t keycode;
                int found = keycode_and_modifer_us_for_character(c, &keycode, &modifier);
                if (!found) continue;

                printf("sending: %c\n", c);

                send_report(modifier, keycode);
                state = W4_CAN_SEND_KEY_UP;
                hids_device_request_can_send_now_event(con_handle);
                break;
            }
            break;
        case W4_CAN_SEND_KEY_UP:
            send_report(0, 0);
            if (btstack_ring_buffer_bytes_available(&key_input_buffer)){
                state = W4_CAN_SEND_FROM_BUFFER;
                hids_device_request_can_send_now_event(con_handle);
            } else {
                state = W4_INPUT;
            }
            break;
        default:
            break;
    }
}

void key_input(char character){
    uint8_t c = character;
    
    btstack_ring_buffer_write(&key_input_buffer, &c, 1);
    // start sending
    if (state == W4_INPUT && con_handle != HCI_CON_HANDLE_INVALID){
        state = W4_CAN_SEND_FROM_BUFFER;
        hids_device_request_can_send_now_event(con_handle);
    }
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = HCI_CON_HANDLE_INVALID;
            printf("Disconnected\n");
            break;
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            printf("Display Passkey: %"PRIu32"\n", sm_event_passkey_display_number_get_passkey(packet));
            break;
        case HCI_EVENT_HIDS_META:
            switch (hci_event_hids_meta_get_subevent_code(packet)){
                case HIDS_SUBEVENT_INPUT_REPORT_ENABLE:
                    con_handle = hids_subevent_input_report_enable_get_con_handle(packet);
                    printf("Report Characteristic Subscribed %u\n", hids_subevent_input_report_enable_get_enable(packet));
                    break;
                case HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE:
                    con_handle = hids_subevent_boot_keyboard_input_report_enable_get_con_handle(packet);
                    printf("Boot Keyboard Characteristic Subscribed %u\n", hids_subevent_boot_keyboard_input_report_enable_get_enable(packet));
                    break;
                case HIDS_SUBEVENT_PROTOCOL_MODE:
                    protocol_mode = hids_subevent_protocol_mode_get_protocol_mode(packet);
                    printf("Protocol Mode: %s mode\n", hids_subevent_protocol_mode_get_protocol_mode(packet) ? "Report" : "Boot");
                    break;
                case HIDS_SUBEVENT_CAN_SEND_NOW:
                    typing_can_send_now();
                    break;
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
}

void key_button_callback(uint gpio, uint32_t events) {
    gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, false);
    gpio_acknowledge_irq(gpio, events);
    
    if (gpio == BACKSPACE_BUTTON ) {       
        busy_wait_ms(100);
        printf("%d\n", events);
        
        key_input(CHAR_BACKSPACE); //Backspace key
        
    }
    if (gpio == RETURN_BUTTON) {
        busy_wait_ms(200);
        key_input(CHAR_RETURN); // return key
        
    }
    gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, true);

}

int init_board(){
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }
    // Example to turn on the Pico W LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    printf("Hello, world!\n");
    return 0;  // 成功を示す
}

void init_pin(){
    for(int i=0;i<row_digit;i++){
        gpio_init(rows[i]);
        gpio_init(columns[i]);
        if(i < 3){
            gpio_init(state_rows[i]);
            gpio_set_dir(state_rows[i], GPIO_IN); //ピンの入出力を設定
            gpio_pull_down(state_rows[i]);
        }
        gpio_set_dir(rows[i], GPIO_IN);//ピンの入出力を設定
        gpio_set_dir(columns[i], GPIO_OUT);
        gpio_put(columns[i], false); //OUTPUTピンの出力を設定 
        // 内部プルダウン抵抗を有効化
        gpio_pull_down(rows[i]);
        if(i < 4){
            gpio_init(joy_sticks[i]);
            gpio_set_dir(joy_sticks[i], GPIO_IN); //ピンの入出力を設定
            gpio_pull_down(joy_sticks[i]);
        }
    }
    // 入力を配列に格納

/*
    joy_states[4] = {0,0,0,0};
    SHIFT_PASS = (0, 0)
    CTRL_PASS = (0, 1)
    NUM_PASS = (0, 2)

    SHIFT_KEY = button_checker[SHIFT_PASS[0]][SHIFT_PASS[1]]
    CTRL_KEY = button_checker[CTRL_PASS[0]][CTRL_PASS[1]]
    NUM_KEY = button_checker[NUM_PASS[0]][NUM_PASS[1]]
*/
}

uint8_t ascii_to_hid_usage_id(char character) {
    // アルファベット（小文字）
    if (character >= 'a' && character <= 'z') {
        return character - 'a' + 0x04; // 'a' -> 0x04, 'b' -> 0x05, ...
    }
    // 特殊文字・記号
    switch (character) {
        case '\n': return 0x28;
        case 0x1b: return 0x29;
        case '\b': return 0x2a;
        case '\t': return 0x2b;
        case ' ': return 0x2C;  // スペース
        case '-': return 0x2D;  // ハイフン
        case '=': return 0x2E;  // イコール
        case '[': return 0x2F;  // 左角括弧
        case ']': return 0x30;  // 右角括弧
        case '\\': return 0x31; // バックスラッシュ
        case ';': return 0x33;  // セミコロン
        case '\'': return 0x34; // アポストロフィ
        case '`': return 0x35;  // グレーブアクセント
        case ',': return 0x36;  // カンマ
        case '.': return 0x37;  // ピリオド
        case '/': return 0x38;  // スラッシュ
        default:
            return character; break;
    }
}

// ここのModifierを変更することで制御文字に対応
void hid_send_ctrl(uint8_t modifier, char character) {
    uint8_t hid_report[HID_REPORT_SIZE] = {0};
    // 修飾キーを設定
    uint8_t keycode = ascii_to_hid_usage_id(character);
    if (keycode == 0 && character != 0) {
        printf("Unsupported character: %c\n", character);
        return;
    }

    if(keycode == CHAR_CHANGE){
        language_checker = !language_checker;
        printf("言語：%d\n", language_checker);
    }
    // 修飾キーを設定
    hid_report[0] = modifier;
    // キーコードを設定
    hid_report[2] = keycode;

    // HID レポートを送信
    // btstack_hid_device_send_report() はスタックに応じた送信関数を使用
    hids_device_send_input_report(con_handle, hid_report, sizeof(hid_report));
}

// 制御文字を取得
void ctrl_check(){
    // ここはmainに合併
    ctrl_counter = 1;
    for(int i=0;i<=4;i++){
        if(button_checker[3][i] != 0){
            ctrl_checker[i] = true;
            hid_send_ctrl(ctrl_raws[i], 0x00);
            ctrl_counter = 0;
            break;
        }else{
            ctrl_checker[i] = false;
        }
    }
    if(ctrl_counter != 0){
        hid_send_ctrl(0x00, 0x00);
    }
    if(button_checker[4][0] != 0){
        num_checker = true;
        printf("num\n");
    }else{
        num_checker = false;
    }
}

void input_checker(int i, int j){
    // numが押されているか
    if(num_checker == true){
        // 10キーなら数字を送る
        if(5 * i + j < 10){
            key_input(return_array_num[5 * i + j]);
            printf("num:%d", 5 * i + j);
        }
    }else if((5 * i + j) < 11){
        // 1文字目が溜まっていないか
        if(count_strager[0] == 11){
            if(ctrl_checker[0] != 0x00 && language_checker == 1){
                hid_send_ctrl(ctrl_raws[0], return_array_jap_ctrl[5 * i + j]);
            }else{
            count_strager[0] = 5 * i + j;
            }
        }else{
            count_strager[1] = 5 * i + j;
            // 1で日本語、0で英語
            if(language_checker == 1){
                // shiftが押されているとき
                if(ctrl_checker[0] != 0x00){
                    hid_send_ctrl(ctrl_raws[0], return_array_jap_shift[count_strager[0]][count_strager[1]]);
                }

                for(int i=0;i<=2;i++){
                    hid_send_ctrl(0x00, return_array_jap[count_strager[0]][count_strager[1]][i]);
                    printf("in:%c\n", return_array_jap[count_strager[0]][count_strager[1]][i]);
                    if(return_array_jap[count_strager[0]][count_strager[1]][i] == None){
                        printf("break\n");
                        break;
                    }
                }
            // 英語入力の時
            }else{
                // shiftが押されているとき
                if(ctrl_checker[1] != 0x00){
                    hid_send_ctrl(ctrl_raws[1], return_array_eng[count_strager[0]][count_strager[1]]);
                }
                    // if(ctrl_counter != 1){
                        // for(int i=0;i<=4;i++){
                        //     if(ctrl_checker[i] != 0x00){
                        //         printf("haitta");
                        //         hid_send_ctrl(ctrl_raws[i], return_array_eng[count_strager[0]][count_strager[1]]);
                        //         // ctrl_counter = 0;
                        //         break;
                        //     }
                        // }
                    // }
                // }
                else{
                    hid_send_ctrl(0x00, return_array_eng[count_strager[0]][count_strager[1]]);
                }
            }
            count_strager[0] = 11;
        }
    }
}

void state_checker(){
    for(int i=0;i<3;i++){
        if(gpio_get(state_rows[i]) == true){
            now_state = i + 1;
            // printf("state:%d", now_state);
        }
    }
}


// キー入力を格納
void key_check(){
    // 入力を保存
    for(int i=0;i<column_digit;i++){
        gpio_put(columns[i], true); //OUTPUTピンの出力を設定 
        for(int j=0;j<row_digit;j++){
            // チャタリング防止のため１周期前の出力を保存
            past_button_checker[i][j] = button_checker[i][j];
            button_checker[i][j] = gpio_get(rows[j]);
            if(button_checker[i][j] != 0 && button_checker[i][j] != past_button_checker[i][j]){
                // 文字入力の場合
                printf("%d行%d列：%d\n",i, j,  button_checker[i][j]);
                if(i < 4){
                    input_checker(i, j);
                }
                // backspace, deleteなど
                else if(i == 4){
                    key_input(return_array_others[i]);
                    printf("others");
                }else{
                // 何もしない
                }
            }
            // 処理が速すぎて検出できないためDelay
            sleep_ms(1);
        }
        gpio_put(columns[i], false); //OUTPUTピンの出力を設定 
    }

    // 処理（Ctrl,Shift,num,fn,back space）があるか
    ctrl_check();
    // number_check();
}

void get_joy(){
    //４方向の状態を読む
    
    //値があるなら配列を上書き
}

void happy_setup() {
 
    //1. initialize ring buffer for key input　キー入力の保存領域を初期化
    btstack_ring_buffer_init(&key_input_buffer, key_input_storage, sizeof(key_input_storage));
    
    //2. l2cap initialize　l2cap:Bluetoothの通信規約(プロトコル)
    l2cap_init();

    //3. setup Security Manager: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

    //4. setup ATT server, ATT protcol:マスタかスレーブかをデバイス同士が確認するためのプロトコル
    // profile_dataの中身はgattファイルをバイナリ化した情報（自動更新？）
    att_server_init(profile_data, NULL, NULL);

    //5. setup battery service
    battery_service_server_init(battery);

    //6. setup device information service
    device_information_service_server_init();

    //7. setup HID Device service
    hids_device_init(0, hid_descriptor_keyboard_boot_mode, sizeof(hid_descriptor_keyboard_boot_mode));

    //8. setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    //9. register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    //10. register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    //11. register for HIDS
    hids_device_register_packet_handler(packet_handler);

    hci_power_control(HCI_POWER_ON);
    }

int main(){
    stdio_init_all();
    init_pin();
    init_board();
    sleep_ms(5000);

  // backspace & return key
    gpio_init(BACKSPACE_BUTTON);
    gpio_init(RETURN_BUTTON);
    gpio_pull_up(BACKSPACE_BUTTON);
    gpio_pull_up(RETURN_BUTTON);
    // 割込み入力を設定　https://www.denshi.club/parts/2021/04/raspberry-pi-pico-16-gpio.html
    gpio_set_irq_enabled_with_callback(BACKSPACE_BUTTON, GPIO_IRQ_EDGE_FALL, true, key_button_callback);
    gpio_set_irq_enabled_with_callback(RETURN_BUTTON, GPIO_IRQ_EDGE_FALL, true, key_button_callback);

    happy_setup();

    hid_send_ctrl(0x00, CHAR_Mu_henkan);

    while (true) {
        state_checker();
        // if (con_handle == HCI_CON_HANDLE_INVALID) continue;
        for(int i=0;i<column_digit;i++){
            gpio_put(columns[i], true); //OUTPUTピンの出力を設定 
            for(int j=0;j<row_digit;j++){
                // チャタリング防止のため１周期前の出力を保存
                past_button_checker[i][j] = button_checker[i][j];
                button_checker[i][j] = gpio_get(rows[j]);
                if(button_checker[i][j] != 0 && button_checker[i][j] != past_button_checker[i][j]){
                    printf("%d行%d列：%d\n",i, j,  button_checker[i][j]);
                    // key_input(CHAR_CHANGE);
                    switch (now_state)
                    {
                    // デフォルトの文字入力
                    case 1:
                        if(i < 4){
                            input_checker(i, j);
                        }
                        // backspace, deleteなど
                        else if(i == 4){
                            hid_send_ctrl(0x00, return_array_others[j]);
                            printf("others");
                        }else{
                        // 何もしない
                        }
                        break;
                    // 左手デバイスとして
                    case 2:
                        /* code */
                        break;  
                    // モールス信号のパターン
                    case 3:
                        /* code */
                        break;  
                    default:
                        if(i < 4){
                            input_checker(i, j);
                        }
                        // backspace, deleteなど
                        else if(i == 4){
                            hid_send_ctrl(0x00, return_array_others[j]);
                            printf("others");
                        }else{
                        // 何もしない
                        }
                        break;
                    }
                }                // 処理が速すぎて検出できないためDelay
                sleep_ms(1);
            }
            gpio_put(columns[i], false); //OUTPUTピンの出力を設定 
        }
        ctrl_check();
    }
    return 0;
}

//  !*#$%':)*+,-./0123456789+;<^>?"ABCDEFGHIJKLMNOPQRSTUVWXYZ@][&=abcdefghijklmnopqrstuvwxyz`}　!*#$%':)*+,-./0123456789+;<^>?"ABCDEFGHIJKLMNOPQRSTUVWXYZ@][&=abcdefghijklmnopqrstuvwxyz`} !*#$%':)*+,-./0123456789+;<^>?"ABCDEFGHIJKLMNOPQRSTUVWXYZ@][&=abcdefghijklmnopqrstuvwxyz`}　
