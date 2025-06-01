
#include <stdio.h>
#include <zephyr/kernel.h>
#include "ipc.hpp"
#include "ultrasonic.hpp"

#define APP_TTY_TASK_STACK_SIZE (1536)
#define APP_ULTRASONIC_TASK_STACK_SIZE (500)
static struct k_thread thread_tty_data;
static struct k_thread thread_ultrasonic_data;
K_THREAD_STACK_DEFINE(thread_tty_stack, APP_TTY_TASK_STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_ultrasonic_stack, APP_TTY_TASK_STACK_SIZE);

int main(void)
{
	k_thread_create(&thread_ultrasonic_data, thread_ultrasonic_stack, APP_ULTRASONIC_TASK_STACK_SIZE, ultrasonic_task, NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);
	k_thread_create(&thread_tty_data, thread_tty_stack, APP_TTY_TASK_STACK_SIZE, ipc_task, NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);

	while (1)
	{
		printf("Distance: %.01f cm\n", get_distance());
		k_sleep(K_MSEC(100));
	}
	
	return 0;
}
