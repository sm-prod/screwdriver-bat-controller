#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <GyverTimer.h>
//#include <avr/io.h>
//#include <avr/interrupt.h>

#define RED_LED		2
#define GREEN_LED	5
#define DS_PIN		4
#define PS_CONTORL	13
#define R_TEMP		0
#define U_BAT		1
const float ref_res = 10000;
const float coef_res = 1.001;//0.552;
const float coef_bat = 2.7622; //2.61*1.02396;
const float min_R_bat = 2000;
const uint16_t max_R_bat = 1000000;
const float min_U_bat = 9.5;
const float max_U_bat = 12.6;
const float min_t_out = 5;
const float max_t_int = 40;

uint8_t flag_bat_ins = 0;
uint8_t flag_start_bat_charge = 0;
uint8_t flag_bat_failed = 0;
uint8_t flag_charge_compleate = 0;

uint8_t cnt_failed_restore = 0;

float get_analog_value(uint8_t type)
{
	float value0 = analogRead(type);
	float value1 = analogRead(type);
	float value2 = analogRead(type);
	float value3 = analogRead(type);
	float value4 = analogRead(type);
	float value5 = analogRead(type);
	float value6 = analogRead(type);
	float value7 = analogRead(type);
	float value8 = analogRead(type);
	float value9 = analogRead(type);
	float value = (((value0 + value1 + value2 + value3 + value4 +
		value5 + value6 + value7 + value8 + value9)/10.0) * 5.0) / 1024.0;
	switch (type)
	{
		case R_TEMP:
			return ref_res * (1.0/((5.0/value)-1)) * coef_res; //((5.0/value)-1)
		case U_BAT:
			return value * coef_bat;
		default:
			return 0;
	}
}

OneWire int_temp_bus(DS_PIN);
DallasTemperature int_temp(&int_temp_bus);
GTimer timer_R_bat(MS, 100);
GTimer timer_red_allert(MS, 500);
GTimer timer_bat_restore(MS, 500);
GTimer timer_green_red_allert(MS, 500);

GTimer timer_test(MS, 1000);

void setup()
{
	pinMode(RED_LED, OUTPUT);
	pinMode(GREEN_LED, OUTPUT);
	pinMode(PS_CONTORL, OUTPUT);

	digitalWrite(RED_LED, HIGH);
	delay(500);
	digitalWrite(GREEN_LED, HIGH);
	delay(500);
	digitalWrite(RED_LED, LOW);
	delay(500);
	digitalWrite(GREEN_LED, LOW);

	int_temp.begin();
	timer_red_allert.stop();
	timer_bat_restore.stop();
	timer_green_red_allert.stop();

	digitalWrite(PS_CONTORL, HIGH);

	Serial.begin(9600);
	timer_test.stop();
	//timer_test2.setTimeout(3000);
}

void loop()
{
	float U_bat = get_analog_value(U_BAT);
	if (timer_R_bat.isReady())
	{
		float R_bat = get_analog_value(R_TEMP);
		if (R_bat < max_R_bat) //Если АКБ вставлена
		{
			//float U_bat = get_analog_value(U_BAT);
			int_temp.requestTemperatures();
			float t_int = int_temp.getTempCByIndex(0);
			if ((R_bat <= min_R_bat) || (t_int < min_t_out) ||
				(t_int > max_t_int)) // Если АКБ сильно нагрета или очень холодно
			{
				digitalWrite(PS_CONTORL, HIGH); // Выключение БП
				timer_red_allert.resume();
			}
			else if (flag_bat_failed)
			{
				digitalWrite(PS_CONTORL, HIGH); // Выключение БП
			}
			else // Если всё впорядке
			{
				if (!flag_start_bat_charge)
				{
					digitalWrite(PS_CONTORL, LOW); // Включение БП
					digitalWrite(RED_LED, HIGH);
					flag_start_bat_charge = 1;
				}
				else if (flag_charge_compleate)
				{
					digitalWrite(PS_CONTORL, HIGH); // Выключение БП
					digitalWrite(RED_LED, LOW);
					digitalWrite(GREEN_LED, HIGH);
				}
			}
		}
		else // Если АКБ нет
		{
			flag_start_bat_charge = 0;
			flag_bat_failed = 0;
			digitalWrite(PS_CONTORL, HIGH); // Выключение БП
			if (timer_red_allert.isEnabled())
				timer_red_allert.stop();
			if (timer_bat_restore.isEnabled())
				timer_bat_restore.stop();
			digitalWrite(RED_LED, LOW);
			digitalWrite(GREEN_LED, LOW);
		}
	}
	if (timer_red_allert.isReady())
		digitalWrite(RED_LED, !digitalRead(RED_LED));
	if (timer_green_red_allert.isReady())
	{
		digitalWrite(RED_LED, !digitalRead(RED_LED));
		digitalWrite(GREEN_LED, !digitalRead(GREEN_LED));
	}
	if (timer_bat_restore.isReady())
	{
		cnt_failed_restore++;
		//float U_bat = get_analog_value(U_BAT);
		if (U_bat <= 7.5)
		{
			cnt_failed_restore = 0;
			timer_bat_restore.stop();
		}
		else if (cnt_failed_restore >= 30)
		{
			cnt_failed_restore = 0;
			flag_bat_failed = 1;
			timer_green_red_allert.resume();
		}
	}

	if (flag_start_bat_charge) // Если активирован режим зарядки
	{
		//float U_bat = get_analog_value(U_BAT);
		if (U_bat < 1 && cnt_failed_restore == 0)
			timer_bat_restore.resume();
		else if (!flag_bat_failed && cnt_failed_restore == 0)
		{
			if (U_bat >= 12.5)
			{
				flag_charge_compleate = 1;
			}
		}
	}


	if (Serial.available() > 4)
		if (Serial.find("DEBUG"))
		{
			if(timer_test.isEnabled())
			{
				timer_test.stop();
				Serial.println("DEBUG stoped");
			}
			else
			{
				timer_test.resume();
				Serial.println("DEBUG started");
			}
		}

	if (timer_test.isReady())
	{
		Serial.print("R battery: ");
		Serial.print(get_analog_value(R_TEMP)/1000);
		Serial.print("k\t");
		Serial.print("U battery: ");
		Serial.print(get_analog_value(U_BAT));
		Serial.print("V\t");
		int_temp.requestTemperatures();
		Serial.print("Internal temp: ");
		Serial.print(int_temp.getTempCByIndex(0));
		Serial.print("C\t");

		if (flag_bat_ins)
			Serial.print("Battery inserted\t");
		if (flag_start_bat_charge)
			Serial.print("Charging started\t");
		if (flag_bat_failed)
			Serial.print("Battery failed\t");
		if (flag_charge_compleate)
			Serial.print("Battery charged\t");
		if (cnt_failed_restore)
		{
			Serial.print("Battery failed count: ");
			Serial.print(cnt_failed_restore);
			Serial.print('\t');
		}
		Serial.print('\n');
	}

}
