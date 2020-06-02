#include <application.h>
#include <list.h>

#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)
#define TEMPERATURE_MEASURE_INTERVAL_SECOND (30)
#define TEMPERATURE_PUBLISH_DELTA (1.0f)
#define TEMPERATURE_PUBLISH_TIMEOUT_SECOND (15 * 60)

// Threshold in milliseconds to differentiate between short and long press
#define BUTTON_PRESS_THRESHOLD 500
// how much time we are allowing for a morse sequence
#define SEQUENCE_MAX_DURATION (30 * 1000)
#define DEFAULT_SEQUENCE ".-."

typedef struct config_t
{
    // Morse code to trigger relay
    char openDoorSequence[64];
    bc_tick_t sequenceTimeout;
} config_t;

config_t config;

config_t config_default = {
    .openDoorSequence = DEFAULT_SEQUENCE,
    .sequenceTimeout = SEQUENCE_MAX_DURATION
};

// Button instance
bc_button_t button;
bc_button_t doorbellButton;
list_t button_history;
list_t pattern;

// LED instance
bc_led_t led;

// Relay instance
static bc_module_relay_t relay;

// Thermometer instance
bc_tmp112_t tmp112;
float publish_temperature = NAN;
bc_tick_t temperature_publish_timeout = 0;

// Counters for button events
uint16_t button_click_count = 0;
uint16_t button_hold_count = 0;

// Time of button has press
bc_tick_t tick_start_button_press;

// Flag for button hold event
bool button_hold_event;

void open_door() 
{
    bc_led_pulse(&led, 750);

    bc_log_info("Toggling relay for 1 sec.");

    bc_module_relay_pulse(&relay, true, 1000);
    //bc_module_relay_toggle(&relay);
}


void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        //bc_led_set_mode(&led, BC_LED_MODE_TOGGLE);
        open_door();
    }

    // Logging in action
    bc_log_info("Button event handler - event: %i", event);
}

// This function dispatches button events
void doorbellButton_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    bool item_added = false;

    if (event == BC_BUTTON_EVENT_CLICK)
    {
        // Pulse LED for 100 milliseconds
        bc_led_pulse(&led, 100);

        // Increment press count
        button_click_count++;

        bc_log_info("APP: Publish button press count = %u", button_click_count);

        // Publish button message on radio
        bc_radio_pub_push_button(&button_click_count);

        // log short press
        //bc_log_info("MORSE: Adding short press");
        list_add_last(&button_history, bc_tick_get(), false);
        item_added = true;
    }
    else if (event == BC_BUTTON_EVENT_HOLD)
    {
        // Pulse LED for 250 milliseconds
        bc_led_pulse(&led, 250);

        // Increment hold count
        button_hold_count++;

        bc_log_info("APP: Publish button hold count = %u", button_hold_count);

        // Publish message on radio
        bc_radio_pub_event_count(BC_RADIO_PUB_EVENT_HOLD_BUTTON, &button_hold_count);

        // Set button hold event flag
        button_hold_event = true;
    }
    else if (event == BC_BUTTON_EVENT_PRESS)
    {
        // Reset button hold event flag
        button_hold_event = false;

        tick_start_button_press = bc_tick_get();
    }
    else if (event == BC_BUTTON_EVENT_RELEASE)
    {
        if (button_hold_event)
        {
            int hold_duration = bc_tick_get() - tick_start_button_press;

            bc_log_info("APP: Publish button hold duration = %d", hold_duration);

            bc_radio_pub_value_int(BC_RADIO_PUB_VALUE_HOLD_DURATION_BUTTON, &hold_duration);

            // log long press
            //bc_log_info("MORSE: Adding long press");
            list_add_last(&button_history, bc_tick_get(), true);
            item_added = true;
        }
    }

    if(item_added) {
        // if our history is longer then expected morse code, remove oldest entry
        if(button_history.length > pattern.length) {
            //bc_log_info("MORSE: Removing oldest entry");
            list_delete_first(&button_history);
        }

        list_print(&button_history);

        if(button_history.length >= pattern.length) {
            bool are_same = list_compare(&pattern, &button_history, config.sequenceTimeout);
            if(are_same) {
                bc_log_info("MORSE: '!!! Patterns are equal -> open the door !!!");
                open_door();
                list_clear(&button_history);
            } 
            //else bc_log_info("MORSE: not equal");
        } 
        //else bc_log_info("MORSE: too short presses, waiting...");
    }
}

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event_param;

    float voltage;

    if (event == BC_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (bc_module_battery_get_voltage(&voltage))
        {
            bc_radio_pub_battery(&voltage);
        }
    }
}

void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param)
{
    float temperature;

    if (event == BC_TMP112_EVENT_UPDATE)
    {
        if (bc_tmp112_get_temperature_celsius(self, &temperature))
        {
            if ((fabsf(temperature - publish_temperature) >= TEMPERATURE_PUBLISH_DELTA) || (temperature_publish_timeout < bc_scheduler_get_spin_tick()))
            {
                bc_log_info("Publishing temperature update.");

                // Used same MQTT topic as for Temperature Tag with alternate i2c address
                bc_radio_pub_temperature(BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE, &temperature);

                publish_temperature = temperature;

                temperature_publish_timeout = bc_tick_get() + (TEMPERATURE_PUBLISH_TIMEOUT_SECOND * 1000);
            }
        }
    }
}

void publish_sequence()
{
    bc_radio_pub_string("config/-/sequence", config.openDoorSequence);
    bc_log_info("Sequence is set to: %s", config.openDoorSequence);
}

void initialize_sequence(char *sample)
{
    list_clear(&pattern);

	size_t i = 0;
	while (sample[i] != '\0') {
		if (sample[i] != '-' && sample[i] != '.')
			continue;

		bool is_long = sample[i] == '-';
		list_add_last(&pattern, 0, is_long);

		i++;
	}
}

void trigger_door_handler(uint64_t *id, const char *topic, void *value, void *param)
{
    //bc_log_info("%s %i", topic, *(int *) value);
    //uint32_t val_uin32 = *(uint32_t *)value;

    open_door();
    
    //char buffer[32];
    //memset(buffer, 0, sizeof(buffer));
    //sprintf(buffer, "%li", val_uin32);
    //bc_radio_pub_string("core/-/door/confirm", buffer);
    bc_radio_pub_string("core/-/door/confirm", "OK");
}

void get_sequence_handler(uint64_t *id, const char *topic, void *value, void *param)
{
    bc_log_info("get_sequence_handler triggered.");
    publish_sequence();
}

void set_sequence_handler(uint64_t *id, const char *topic, void *value, void *param)
{
    bc_log_info("set_sequence_handler triggered.");
    
    snprintf(config.openDoorSequence, sizeof(config.openDoorSequence), "%s", (char*)value);
    initialize_sequence(config.openDoorSequence);
    bc_config_save();

    publish_sequence();
}
/*
void publish_timeout()
{
    bc_radio_pub_int("config/-/timeout", config.sequenceTimeout);
    bc_log_info("Sequence timeout is set to %llu ms.", config.sequenceTimeout);
}

void get_timeout_handler(uint64_t *id, const char *topic, void *value, void *param)
{
    bc_log_info("get_timeout_handler triggered.");
    publish_timeout();
}
*/
static const bc_radio_sub_t subs[] = {
    {"core/-/door/open", BC_RADIO_SUB_PT_NULL, trigger_door_handler, NULL},
    {"config/-/sequence/set", BC_RADIO_SUB_PT_STRING, set_sequence_handler, NULL},
    {"config/-/sequence/get", BC_RADIO_SUB_PT_NULL, get_sequence_handler, NULL},
//    {"config/-/timeout/set", BC_RADIO_SUB_PT_STRING, set_timeout_handler, NULL},
//    {"config/-/timeout/get", BC_RADIO_SUB_PT_NULL, get_timeout_handler, NULL},
};

void application_init(void)
{
    bc_config_init(0xd00be11, &config, sizeof(config), &config_default);

    // Initialize logging
    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_ON);

    // Initialize button history matching
    list_init(&button_history);
    list_init(&pattern);

    // Load pattern from config
    bc_log_info("initalizing pattern from config: %s", config.openDoorSequence);
	initialize_sequence(config.openDoorSequence);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize doorbell button
    bc_button_init(&doorbellButton, BC_GPIO_P17, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&doorbellButton, doorbellButton_event_handler, NULL);
    bc_button_set_click_timeout(&doorbellButton, BUTTON_PRESS_THRESHOLD);
    bc_button_set_hold_time(&doorbellButton, BUTTON_PRESS_THRESHOLD);
    
    // Initialize relay
    bc_module_relay_init(&relay, BC_MODULE_RELAY_I2C_ADDRESS_DEFAULT);
    bc_module_relay_set_state(&relay, false);

    // Initialize battery
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize thermometer sensor on core module
    bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);
    bc_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    bc_tmp112_set_update_interval(&tmp112, TEMPERATURE_PUBLISH_TIMEOUT_SECOND);

    // Initialize radio
    bc_radio_init(BC_RADIO_MODE_NODE_LISTENING);
    bc_radio_set_subs((bc_radio_sub_t *) subs, sizeof(subs)/sizeof(bc_radio_sub_t));

    bc_radio_pairing_request("doorbell", VERSION);

    bc_led_pulse(&led, 2000);

    // publish current sequence
    publish_sequence();
}

/*

void application_init(void)
{
    // Initialize radio communication
    bc_radio_init(BC_RADIO_MODE_NODE_LISTENING);
    //bc_radio_listen(1000);
    bc_radio_set_subs((bc_radio_sub_t *) subs, sizeof(subs)/sizeof(bc_radio_sub_t));
    bc_radio_set_event_handler(radio_event_handler, NULL);

    bc_scheduler_plan_current_relative(2000);
}

void application_task(void)
{
    // Logging in action
    bc_log_debug("application_task run");

    //bc_led_set_mode(&led, BC_LED_MODE_ON);
    //bc_radio_listen(1000);
    //bc_led_set_mode(&led, BC_LED_MODE_OFF);
    //bc_radio_sleep();

    bc_led_pulse(&led, 500);

    // Plan next run this function after 10000 ms
    //bc_scheduler_plan_current_from_now(10000);
    bc_scheduler_plan_current_relative(200);
}
*/
