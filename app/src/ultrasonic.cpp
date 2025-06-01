#include "ultrasonic.hpp"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
LOG_MODULE_REGISTER(ultrasonic);

static const struct device *ultrasonic = DEVICE_DT_GET_ONE(hc_sr04);
constexpr int messure_count = 10;
float distance[messure_count];


static int trigger_measurement(float &range)
{
	int ret;
	static struct sensor_value distance = {-1, 0};
	
	ret = sensor_sample_fetch(ultrasonic);

	if (0 == ret)
	{
		ret = sensor_channel_get(ultrasonic, SENSOR_CHAN_DISTANCE, &distance);
		
		if (0 == ret)
		{
			range = sensor_value_to_float(&distance);
		}
		else
		{
			LOG_ERR("%s - Cannot take measurement: %d", ultrasonic->name, ret);
		}
	}
	else
	{
		LOG_ERR("%s - Cannot take measurement: %d", ultrasonic->name, ret);
	}

	return ret;
}

float get_distance()
{
	float result = 0.f;

	for (auto &&val : distance)
	{
		result += val;
	}
	
	return result / messure_count *100.f;
}

static void run(void)
{
	static int idx = 0;

	if (0 == trigger_measurement(distance[idx]))
	{
		idx++;
	}

	if (messure_count == idx)
	{
		idx = 0;
	}
}

void ultrasonic_task(void *arg1, void *arg2, void *arg3)
{
	if (!device_is_ready(ultrasonic)) {
		LOG_ERR("Device %s is not ready\n", ultrasonic->name);
		return;
	}

	while (1)
	{
		run();
		k_sleep(K_MSEC(20));
	}
}