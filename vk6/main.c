#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/timing/timing.h>
#include <assert.h>
#include <ctype.h>

/* //Viikkotehtävä 6 2p suoritus
* Aikamerkkijonon testaus UARTin kautta robot frameworkilla
* 
* 
* 
*
*/

//#define DEBUG
bool debug = false;

// Condition Variables
K_MUTEX_DEFINE(red_mutex);
K_CONDVAR_DEFINE(red_signal);
K_MUTEX_DEFINE(green_mutex);
K_CONDVAR_DEFINE(green_signal);
K_MUTEX_DEFINE(yellow_mutex);
K_CONDVAR_DEFINE(yellow_signal);
K_MUTEX_DEFINE(release_mutex);
K_CONDVAR_DEFINE(release_signal);

// Timer initializations
struct k_timer timer;
void timer_handler(struct k_timer *timer_id);
int timer_delay = 0;

#define TIME_LEN_ERROR      -1
#define TIME_ARRAY_ERROR    -2
#define TIME_VALUE_ERROR    -3
#define TIME_ZERO_ERROR     -4
#define TIME_DIGIT_ERROR    -5

// UART initialization
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

// Create dispatcher FIFO buffer
K_FIFO_DEFINE(dispatcher_fifo);
K_FIFO_DEFINE(debug_fifo);

// Dispatcher data in FIFO
struct data_t {
	void *fifo_reserved; // Reserved for Zephyr's internal use
	char msg[20];
};

// Debug data in FIFO
struct debug_data_t {
	void *fifo_reserved;
	uint64_t time;
	char msg[20];
	int id;
};

// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

// Thread initialization
void red_led_task(void *, void *, void*);
void green_led_task(void *, void *, void*);
void yellow_led_task(void *, void *, void*);
static void dispatcher_task(void *, void *, void*);
static void uart_task(void *, void *, void*);
void debug_task(void *, void *, void*);
#define STACKSIZE 1024
#define PRIORITY 5
K_THREAD_DEFINE(red_thread,STACKSIZE,red_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(green_thread,STACKSIZE,green_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(yellow_thread,STACKSIZE,yellow_led_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(dis_thread,STACKSIZE,dispatcher_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(uart_thread,STACKSIZE,uart_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(debug_thread,STACKSIZE,debug_task,NULL,NULL,NULL,PRIORITY,0,0);

int led_state = 0; // 0 = red, 1 = yellow, 2 = green, 3 = blink yellow 4 = stop
bool stopFlag = false;
int mem_led = -1;

bool red_led = false; //btn1
bool green_led = false; //btn2
bool yellow_led = false; //btn3
bool yellow_blinking = false;
static bool stop_requested = false;

//Sequence time and count
uint64_t seq_time = 0;
int seq_cnt = 0;
int led_task_cnt = 0;

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

void basic_sequence() {
	
	char sequence[20] = "RYGYRYGT,1";
	struct data_t *buf = k_malloc(sizeof(struct data_t));
	if (buf == NULL) {
		return;
	}
	snprintf(buf->msg, 20, "%s", sequence);
	k_fifo_put(&dispatcher_fifo, buf);

}

// control led using timer interrupt
void timer_handler(struct k_timer *timer_id) {
	//printk("Timer handler\n");
	struct data_t *buf = k_malloc(sizeof(struct data_t));
	if (buf == NULL) {
		return;
	}
	snprintf(buf->msg, 20, "%s", "R,T,5");
	k_fifo_put(&dispatcher_fifo, buf);
}

// time format: HHMMSS (6 characters)
int time_parse(char *time) {

	// how many seconds, default returns error
	int seconds = TIME_LEN_ERROR;

	// TODO: Check that string is not null
	if (time == NULL) {
		return TIME_ARRAY_ERROR;
	}
	// Check that string length is correct
	else if (strlen(time) != 6) {
		return TIME_LEN_ERROR;
	}
	// Check that all characters are digits
	if (!isdigit(time[0]) || !isdigit(time[1]) || !isdigit(time[2])) {
		return TIME_DIGIT_ERROR;
	}

	// Parse values from time string
	// For example: 124033 -> 12hour 40min 33sec
    int values[3];
	values[2] = atoi(time+4); // seconds
	time[4] = 0;
	values[1] = atoi(time+2); // minutes
	time[2] = 0;
	values[0] = atoi(time); // hours
	// Now you have:
	// values[0] hour
	// values[1] minute
	// values[2] second

	// TODO: Add boundary check time values: below zero or above limit not allowed
	// limits are 59 for minutes, 23 for hours, etc
	if (values[0] < 0 || values[0] > 23) {
		return TIME_VALUE_ERROR;
	}
	if (values[1] < 0 || values[1] > 59) {
		return TIME_VALUE_ERROR;
	}
	if (values[2] < 0 || values[2] > 59) {
		return TIME_VALUE_ERROR;
	}

	// TODO: Calculate return value from the parsed minutes and seconds
	// Otherwise error will be returned!
	// seconds = ...
	seconds = values[0] * 3600 + values[1] * 60 + values[2];

	// Check that calculated time is not 0 or negative
	if (seconds <= 0) {
		return TIME_ZERO_ERROR;
	}


	return seconds;
}

// Button interrupt handler
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	//printk("Button 0 pressed\n");
	if(stopFlag) {
		printk("Pause OFF\n");
		stopFlag = false;
		led_task_cnt = 0;
		seq_cnt = 0;
		//led_state = mem_led; // palautetaan ledin tila
		if (led_state == 0) {
			k_condvar_broadcast(&red_signal);
		} 
		else if (led_state == 1) {
			k_condvar_broadcast(&yellow_signal);
		} 
		else if (led_state == 2) {
			k_condvar_broadcast(&green_signal);
		} 
	} else {
		//mem_led = led_state; //ledin tila talteen
		printk("Pause ON\n");
		stopFlag = true;
		all_leds_off();
		//Update sequence counter
		seq_cnt = led_task_cnt + 1; 
	}
}
void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 1 pressed\n");
	struct data_t *buf = k_malloc(sizeof(struct data_t));
	if (buf == NULL) {
		return;
	}
	snprintf(buf->msg, 20, "%s", "R,T,1");
	k_fifo_put(&dispatcher_fifo, buf);
}
void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 2 pressed\n");
	struct data_t *buf = k_malloc(sizeof(struct data_t));
	if (buf == NULL) {
		return;
	}
	snprintf(buf->msg, 20, "%s", "Y,T,1");
	k_fifo_put(&dispatcher_fifo, buf);

}
void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("Button 3 pressed\n");
	struct data_t *buf = k_malloc(sizeof(struct data_t));
	if (buf == NULL) {
		return;
	}
	snprintf(buf->msg, 20, "%s", "G,T,1");
	k_fifo_put(&dispatcher_fifo, buf);

}
void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	printk("BTN 5 pressed\n");
	
	///basic_sequence();
	//Blink yellow
	struct data_t *buf = k_malloc(sizeof(struct data_t));
	if (buf == NULL) {
		return;
	}

	if(!yellow_blinking) {
		snprintf(buf->msg, 20, "%s", "Y,T,100");
		k_fifo_put(&dispatcher_fifo, buf);
		yellow_blinking = true;
	} else {
		stop_requested = true; //Stop flag to dispatcher task
		yellow_blinking = false;
	}

}

int init_uart(void) {
	// UART initialization
	if (!device_is_ready(uart_dev)) {
		return 1;
	} 
	return 0;
}
// Main program
int main(void)
{
	init_led();
	timing_init();

	// Timer initialization
	k_timer_init(&timer, timer_handler, NULL);

	int ret = init_button();
	if (ret < 0) {
		return 0;
	}
	//UART
	ret = init_uart();
	if (ret != 0) {
		printk("UART initialization failed!\n");
		return ret;
	}

	// Wait for everything to initialize and threads to start
	k_msleep(100);
	
	//Pause on
	stopFlag = true;
	led_state = 0;

/* 	printk("Press BTN1 to start/stop the continuous sequence.\n");
	printk("Press BTN2 to blink the RED led once\n");
	printk("Press BTN3 to blink the YELLOW led once\n");
	printk("Press BTN4 to blink the GREEN led once\n");
	printk("Press BTN5 to start/stop continuous blinking of the YELLOW led\n");
	printk("Or enter a sequence (e.g., RYG,T,5) to repeat the sequence 5 times\n");
	printk("Enter D to toggle debug mode (on/off)\n");
	printk("Enter A to set timer delay HHMMSS\n"); */

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
	//printk("Set up button 0 ok\n");

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
	//printk("Set up button 1 ok\n");

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
	//printk("Set up button 2 ok\n");

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
	//printk("Set up button 3 ok\n");

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
	//printk("Set up button 4 ok\n");

	return 0;

}

// Initialize leds
int init_led() {

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

	//printk("Led initialized ok\n");
}

// Task to handle red led
void red_led_task(void *, void *, void*) {
	
	while (true) {
		led_state = 0;
		// Wait for signal
		k_condvar_wait(&red_signal, &red_mutex, K_FOREVER);

		struct debug_data_t *buf = k_malloc(sizeof(struct debug_data_t));
		if (buf == NULL) {
			return;
		}

		timing_start();
		timing_t start_time = timing_counter_get();
		// 1. set led on 
		gpio_pin_set_dt(&red,1);

		if (debug){
			//printk("Red on\n");
			buf->time = 0;
			sprintf(buf->msg, "Red on");
			k_fifo_put(&debug_fifo, buf);
		}

		k_msleep(1000);
		
		// 3. set led off
		gpio_pin_set_dt(&red,0);

		//printk("Red off\n");
		buf = k_malloc(sizeof(struct debug_data_t));
		if (buf == NULL) {
			return;
		}
		buf->time = 0;
		sprintf(buf->msg, "Red off");
		k_fifo_put(&debug_fifo, buf);


		k_msleep(1000);

		//Release signal to dispatcher task
		k_condvar_broadcast(&release_signal);
		
		if(!stopFlag){
			k_condvar_broadcast(&yellow_signal);
		}

		timing_stop();
		timing_t end_time = timing_counter_get();
        uint64_t diff = timing_cycles_to_ns(timing_cycles_get(&start_time, &end_time) / 1000);

		led_task_cnt++;

		buf = k_malloc(sizeof(struct debug_data_t));
		if (buf == NULL) {
			return;
		}
		buf->time = diff;
		buf->id = 1;
		k_fifo_put(&debug_fifo, buf);
		// printk("Red added to fifo: %lld\n",buf->time);	
	}
	
}

// Task to handle green led
void green_led_task(void *, void *, void*) {
	
	while (true) {
		led_state = 2;
		// Wait for signal
		k_condvar_wait(&green_signal, &green_mutex, K_FOREVER);
		
		struct debug_data_t *buf = k_malloc(sizeof(struct debug_data_t));
		if (buf == NULL) {
			return;
		}

		timing_start();
		timing_t start_time = timing_counter_get();
		// 1. set led on 
		gpio_pin_set_dt(&green,1);

		//printk("Green on\n");
		buf->time = 0;
		sprintf(buf->msg, "Green on");
		k_fifo_put(&debug_fifo, buf);

		k_msleep(1000);
		
		// 3. set led off
		gpio_pin_set_dt(&green,0);


		//printk("Green off\n");
		buf = k_malloc(sizeof(struct debug_data_t));
		if (buf == NULL) {
			return;
		}
		buf->time = 0;
		sprintf(buf->msg, "Green off");
		k_fifo_put(&debug_fifo, buf);

		k_msleep(1000);


		//Release signal to dispatcher task
		k_condvar_broadcast(&release_signal);
		
		if(!stopFlag){
			k_condvar_broadcast(&red_signal);
		}

		timing_stop();
		timing_t end_time = timing_counter_get();
        uint64_t diff = timing_cycles_to_ns(timing_cycles_get(&start_time, &end_time) / 1000);

		led_task_cnt++;

		buf = k_malloc(sizeof(struct debug_data_t));
		if (buf == NULL) {
			return;
		}	
		buf->time = diff;
		buf->id = 2;
		k_fifo_put(&debug_fifo, buf);
		// printk("Green added to fifo: %lld\n",buf->time);
	}
	
}

// Task to handle yellow led
void yellow_led_task(void *, void *, void*) {
	
	while (true) {
		led_state = 1;
		// Wait for signal
		k_condvar_wait(&yellow_signal, &yellow_mutex, K_FOREVER);

 		struct debug_data_t *buf = k_malloc(sizeof(struct debug_data_t));
		if (buf == NULL) {
			return;
		}

		timing_start();
		timing_t start_time = timing_counter_get();
		// 1. set led on 
		gpio_pin_set_dt(&green,1);
		gpio_pin_set_dt(&red,1);

		//printk("Yellow on\n");
		buf->time = 0;
		sprintf(buf->msg, "Yellow on");
		k_fifo_put(&debug_fifo, buf);

		k_msleep(1000);
		
		// 3. set led off
		gpio_pin_set_dt(&green,0);
		gpio_pin_set_dt(&red,0);

		
		//printk("Yellow off\n");
		buf = k_malloc(sizeof(struct debug_data_t)); 
		if (buf == NULL) {
			return;
		}
		buf->time = 0;
		sprintf(buf->msg, "Yellow off");
		k_fifo_put(&debug_fifo, buf);
		

		k_msleep(1000);

		//Release signal to dispatcher task
		k_condvar_broadcast(&release_signal);

		if(!stopFlag){
			k_condvar_broadcast(&green_signal);
		}
		
		timing_stop();
		timing_t end_time = timing_counter_get();
        uint64_t diff = timing_cycles_to_ns(timing_cycles_get(&start_time, &end_time) / 1000);

		led_task_cnt++;

		buf = k_malloc(sizeof(struct debug_data_t));
		if (buf == NULL) {
			return;
		}
		buf->time = diff;
		buf->id = 3;
		k_fifo_put(&debug_fifo, buf);
		//printk("Yellow added to fifo: %lld\n",buf->time);
	}
	
}

/********************
 * UART task
 */
static void uart_task(void *unused1, void *unused2, void *unused3)
{
	// Received character from UART
	char rc=0;
	// Message from UART
	char uart_msg[20];
	memset(uart_msg,0,20);
	int uart_msg_cnt = 0;

	while (true) {
		// Ask UART if data available
		if (uart_poll_in(uart_dev,&rc) == 0) {
			// printk("Received: %c\n",rc);
			// If character is not newline, add to UART message buffer
			if (rc != '\r') {
				uart_msg[uart_msg_cnt] = rc;
				uart_msg_cnt++;
			// Character is newline, copy dispatcher data and put to FIFO buffer
			} else {
				//printk("UART msg: %s\n", uart_msg);

				//Test if time string
				if (isdigit(uart_msg[0])) {
					timer_delay = time_parse(uart_msg);
					//printk("Timer delay: %d\n", timer_delay);
					k_timer_start(&timer, K_SECONDS(timer_delay), K_SECONDS(0));

					uart_msg_cnt = 0;
					memset(uart_msg,0,20);

					// Echo back the timer delay string to the UART for the Robot Framework test
					char timer_delay_str[20];
					snprintf(timer_delay_str, 20, "%d", timer_delay);
					
					//k_msleep(100);
					for (int i = 0; timer_delay_str[i] != '\0'; i++) {
						uart_poll_out(uart_dev, timer_delay_str[i]); // Send character by character
					}


				} else {
					//Sequence
					struct data_t *buf = k_malloc(sizeof(struct data_t));
					if (buf == NULL) {
						return;
					}
					snprintf(buf->msg, 20, "%s", uart_msg);
					//printk("Buffer msg: %s\n", buf->msg);
					// You need to:
					// Put dispatcher data to FIFO buffer
					k_fifo_put(&dispatcher_fifo, buf);
					// Clear UART receive buffer
					uart_msg_cnt = 0;
					memset(uart_msg,0,20);
					// Clear UART message buffer
					uart_msg_cnt = 0;
					memset(uart_msg,0,20);
				}
			}
		}
		k_msleep(10);
	}
	
}

/********************
 * Dispatcher task
 */
static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
	while (true) {
		// Receive dispatcher data from uart_task fifo
		struct data_t *rec_item = k_fifo_get(&dispatcher_fifo, K_FOREVER);
		char sequence[20];
		memcpy(sequence,rec_item->msg,20);
		k_free(rec_item);

		//printk("Dispatcher: %s\n", sequence);

		if (strcmp(sequence, "D") == 0) {
            // Toggle debug mode
            debug = !debug;
            printk("Debug mode %s\n", debug ? "on" : "off");
            continue;
        }

		// Find the position of 'T' in the sequence
        char *t_pos = strchr(sequence, 'T');
        if (t_pos == NULL) {
            //printk("Invalid sequence format\n");
            continue;
        }
		assert(t_pos != NULL);

        // Parse the repeat count
        int repeat_count = atoi(t_pos + 2); // Assuming format is T,<number>
        if (repeat_count <= 0) {
            //printk("Invalid repeat count\n");
            continue;
        }
		assert(repeat_count > 0);

        // Null-terminate the sequence at 'T'
        *t_pos = '\0';

        // Repeat the sequence
        for (int repeat = 0; repeat < repeat_count; repeat++) {
			if (stop_requested) {
                printk("Sequence stopped\n");
                break;
            }

            for (int i = 0; i < strlen(sequence); i++) {
				if (stop_requested) {
                	break;
           		}
                if (sequence[i] == 'R') {
                    k_condvar_broadcast(&red_signal);
					seq_cnt++;
                }
                else if (sequence[i] == 'G') {
                    k_condvar_broadcast(&green_signal);
					seq_cnt++;
                }
                else if (sequence[i] == 'Y') {
                    k_condvar_broadcast(&yellow_signal);
					seq_cnt++;
                } else {
                    break;				
                }
                k_condvar_wait(&release_signal, &release_mutex, K_FOREVER);
                //printk("release received\n");
            }
		}
		stop_requested = false;
		k_yield();
        // You need to:
        // Parse color and time from the fifo data
        // Example
        //    char color = sequence[0];
        //    int time = atoi(sequence+2);
		//    printk("Data: %c %d\n", color, time);
        // Send the parsed color information to tasks using fifo
        // Use release signal to control sequence or k_yield
	}
}

void debug_task(void *, void *, void*) {

	// Store received data
	struct debug_data_t *received;
	
	while (true) {

		received = k_fifo_get(&debug_fifo, K_FOREVER);

		if(debug){
			//Print task debug message
			if(received->time == 0) {	
				printk("[DEBUG-task] message: %s\n", received->msg);
				assert(received->msg != NULL);
			}else {
				//Print task time
				switch(received->id) {
					assert(received->id >= 1 && received->id <= 3);
				case 1:
					printk("[DEBUG-task] Red task time: %lld\n", received->time);
					break;
				case 2:
					printk("[DEBUG-task] Green task time: %lld\n", received->time);
					break;
				case 3:
					printk("[DEBUG-task] Yellow task time: %lld\n", received->time);
					break;
				default:
					//printk("Unknown task id: %d\n", received->id);
					break;
				}

				//Calculate sequence time
				seq_time += received->time;
				//printk("Led task count: %d\n", led_task_cnt);
				//printk("Sequence count: %d\n", seq_cnt);

				if(led_task_cnt == seq_cnt) {
					printk("[DEBUG-task] Total sequence time: %lld\n", seq_time);
					led_task_cnt = 0;
					seq_time = 0;
					seq_cnt = 0;
				}
			}
		}
		
		k_free(received);
		k_yield();
	}
}

