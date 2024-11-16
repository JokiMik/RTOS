#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>


//Viikon 2 kotitehtävä 3 pisteen toteutus


// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

// Led thread initialization
void red_led_task(void *, void *, void*);
void green_led_task(void *, void *, void*);
void yellow_led_task(void *, void *, void*);
#define STACKSIZE 500
#define PRIORITY 5
K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);

int led_state = 0; // 0 = red, 1 = yellow, 2 = green, 3 = blink yellow 4 = stop
bool stopFlag = false;
int mem_led = -1;

bool red_led = false;
bool green_led = false;
bool yellow_led = false;

// Configure buttons
#define BUTTON_0 DT_ALIAS(sw0) //pause
#define BUTTON_1 DT_ALIAS(sw1) //red on/off
#define BUTTON_2 DT_ALIAS(sw2) //yellow on/off
#define BUTTON_3 DT_ALIAS(sw3) //green on/off
#define BUTTON_4 DT_ALIAS(sw4) //blink yellow
static const struct gpio_dt_spec button_0 = GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});
static struct gpio_callback button_0_data;
static const struct gpio_dt_spec button_1 = GPIO_DT_SPEC_GET_OR(BUTTON_1, gpios, {1});
static struct gpio_callback button_1_data;
static const struct gpio_dt_spec button_2 = GPIO_DT_SPEC_GET_OR(BUTTON_2, gpios, {2});
static struct gpio_callback button_2_data;
static const struct gpio_dt_spec button_3 = GPIO_DT_SPEC_GET_OR(BUTTON_3, gpios, {3});
static struct gpio_callback button_3_data;
static const struct gpio_dt_spec button_4 = GPIO_DT_SPEC_GET_OR(BUTTON_4, gpios, {4});
static struct gpio_callback button_4_data;

void all_leds_off() {
	gpio_pin_set_dt(&red,0);
	gpio_pin_set_dt(&green,0);
	gpio_pin_set_dt(&blue,0);
	red_led = false;
	green_led = false;
	yellow_led = false;
}

// Button interrupt handler
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 0 pressed\n");
	if(stopFlag) {
		stopFlag = false;
		led_state = mem_led; // palautetaan ledin tila

	} else {
		mem_led = led_state; //ledin tila talteen
		led_state = 4;
		stopFlag = true;
		gpio_pin_set_dt(&red,0);
		gpio_pin_set_dt(&green,0);

	}
}
void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 1 pressed\n");
	if (red_led) {
		red_led = false;
		all_leds_off();

	} else {
		all_leds_off();
		red_led = true;
		gpio_pin_set_dt(&red,1);
	}
}
void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 2 pressed\n");
	if(yellow_led) {
		yellow_led = false;
		all_leds_off();	
	} else {
		all_leds_off();
		yellow_led = true;
		gpio_pin_set_dt(&red,1);
		gpio_pin_set_dt(&green,1);
	}
}
void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 3 pressed\n");
	if (green_led) {
		green_led = false;
		all_leds_off();

	} else {
		all_leds_off();
		green_led = true;
		gpio_pin_set_dt(&green,1);
	}
}
void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 4 pressed\n");
	if(led_state == 3) {
		led_state = 4;
		
	} else {
		led_state = 3;
	}

}

// Main program
int main(void)
{
	init_led();

	int ret = init_button();
	if (ret < 0) {
		return 0;
	}

	return 0;
}
// Button initialization
int init_button() {

	int ret;
	if (!gpio_is_ready_dt(&button_0)) {
		printk("Error: button 0 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_0, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_0, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_0_data, button_0_handler, BIT(button_0.pin));
	gpio_add_callback(button_0.port, &button_0_data);
	printk("Set up button 0 ok\n");

	//button1
	if (!gpio_is_ready_dt(&button_1)) {
		printk("Error: button 1 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_1, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_1, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_1_data, button_1_handler, BIT(button_1.pin));
	gpio_add_callback(button_1.port, &button_1_data);
	printk("Set up button 1 ok\n");

	//button2
	if (!gpio_is_ready_dt(&button_2)) {
		printk("Error: button 2 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_2, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_2, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_2_data, button_2_handler, BIT(button_2.pin));
	gpio_add_callback(button_2.port, &button_2_data);
	printk("Set up button 2 ok\n");

	//button3
	if (!gpio_is_ready_dt(&button_3)) {
		printk("Error: button 3 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_3, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_3, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_3_data, button_3_handler, BIT(button_3.pin));
	gpio_add_callback(button_3.port, &button_3_data);
	printk("Set up button 3 ok\n");

	//button4
	if (!gpio_is_ready_dt(&button_4)) {
		printk("Error: button 4 is not ready\n");
		return -1;
	}

	ret = gpio_pin_configure_dt(&button_4, GPIO_INPUT);
	if (ret != 0) {
		printk("Error: failed to configure pin\n");
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&button_4, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error: failed to configure interrupt on pin\n");
		return -1;
	}

	gpio_init_callback(&button_4_data, button_4_handler, BIT(button_4.pin));
	gpio_add_callback(button_4.port, &button_4_data);
	printk("Set up button 4 ok\n");

}

// Initialize leds
void init_led() {

	// Led pin initialization
	int ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}
	// set led off
	gpio_pin_set_dt(&red,0);

	// Led pin initialization
	ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}
	// set led off
	gpio_pin_set_dt(&green,0);

	printk("Led initialized ok\n");
}

// Task to handle red led
void red_led_task(void *, void *, void*) {
	
	printk("Red led thread started\n");
	while (true) {
		if (led_state == 0) {
			printk("%d\n",led_state);
			// 1. set led on 
			gpio_pin_set_dt(&red,1);
			printk("Red on\n");
			// 2. sleep for 2 seconds
			//k_sleep(K_SECONDS(1));
			k_msleep(1000);
			// 3. set led off
			gpio_pin_set_dt(&red,0);
			printk("Red off\n");
			// 4. sleep for 2 seconds
			///k_sleep(K_SECONDS(1));
			k_msleep(500);
			if (!stopFlag) {
				led_state = 1;
			}
		}
		k_msleep(100);
	}
	
}

// Task to handle green led
void green_led_task(void *, void *, void*) {
	
	printk("Green led thread started\n");
	while (true) {
		if(led_state == 2) {
			printk("%d\n",led_state);
			// 1. set led on 
			gpio_pin_set_dt(&green,1);
			printk("Green on\n");
			// 2. sleep for 2 seconds
			k_msleep(1000);
			// 3. set led off
			gpio_pin_set_dt(&green,0);
			printk("Green off\n");
			// 4. sleep for 2 seconds
			k_msleep(500);
			if (!stopFlag) {
				led_state = 0;
			}
		}
		k_msleep(100);
	}
	
}

// Task to handle yellow led
void yellow_led_task(void *, void *, void*) {
	
	printk("Yellow led thread started\n");
	while (true) {
		if(led_state == 1 || led_state == 3) { //led_state == 3 blink yellow
			printk("%d\n",led_state);
			// 1. set led on 
			gpio_pin_set_dt(&green,1);
			gpio_pin_set_dt(&red,1);
			printk("Yellow on\n");
			// 2. sleep for 2 seconds
			k_msleep(1000);
			// 3. set led off
			gpio_pin_set_dt(&green,0);
			gpio_pin_set_dt(&red,0);
			printk("Yellow off\n");
			// 4. sleep for 2 seconds
			k_msleep(500);
			if (!stopFlag || !led_state == 3) {
				led_state = 2;
			}
		}
		k_msleep(100);
	}
	
}