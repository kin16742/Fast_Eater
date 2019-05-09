#include "includes.h"

#define F_CPU	16000000UL
#include <avr/io.h>	
#include <util/delay.h>
#include <stdio.h>

#define  TASK_STK_SIZE  OS_TASK_DEF_STK_SIZE            
#define  N_TASKS        4
#define  MSG_QUEUE_SIZE 4

#define ON 1
#define OFF 0

OS_STK         TaskStk[N_TASKS][TASK_STK_SIZE];
OS_EVENT	  *Mbox;
OS_EVENT      *Sem;
OS_EVENT	  *MQueue;
OS_FLAG_GRP   *f_grp;

void *MQueue_Table[MSG_QUEUE_SIZE];

volatile int state = OFF;
volatile int flag = 0;
volatile unsigned char curr = 7, meal = 7;
volatile int cnt = -1;
volatile int idx = 0;
volatile int sound = -1;


unsigned char FND_DATA[] = {
	0x3f, // 0
	0x06, // 1 
	0x5b, // 2 
	0x4f, // 3 
	0x66, // 4 
	0x6d, // 5 
	0x7d, // 6 
	0x27, // 7 
	0x7f, // 8 
	0x6f, // 9 
	0x77, // A 
	0x7c, // B 
	0x39, // C 
	0x5e, // D 
	0x79, // E 
	0x71, // F 
	0x80, // . 
	0x40, // - 
	0x08, // _
	0x00
};
unsigned char FND_sel[4] = { 0x01,0x02,0x04,0x08 };
unsigned char LED_sel[8] = { 0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80 };
unsigned char stage = 0;
int timer[3] = { 4000,3000,2000 };

ISR(TIMER2_OVF_vect);
ISR(INT4_vect);
ISR(INT5_vect);
void  StageClearTask(void *data);
void  AllClearTask(void *data);
void  DeadTask(void *data);
void  DisplayTask(void *data);

//{ C17,D43,E66,F77,G97,H114,A#121,B129,C137,D144 };
int eat[3] = { 
	137,97,-1 
};
int clear[10] = { 
	17,66,97,137,-1,97,137,-1,-1,-1 
};
int dead[14] = { 
	66,-1,66,-1,66,-1,17,17,17,17,17,17,17,-1 
};
int full[96] = {
	17,-1,-1,43,-1,-1,17,-1,-1,97,-1,-1,-1,77,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	77,-1,-1,97,-1,-1,77,-1,-1,137,-1,-1,-1,121,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	114,-1,114,-1,-1,114,-1,137,-1,150,-1,-1,114,-1,137,-1,
	137,-1,97,-1,114,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	17,-1,43,-1,17,-1,43,-1,17,-1,114,-1,77,-1,-1,-1
};

int main(void) {
	INT8U err;

	OSInit();

	OS_ENTER_CRITICAL();
	DDRA = 0xff;
	DDRC = 0xff;
	DDRG = 0x0f;
	DDRB = 0x10;
	DDRE = 0xcf;
	EICRB = 0x0A;
	EIMSK = 0x30;
	TCCR0 = 0x07;
	TCCR2 = 0x03;
	TIMSK = _BV(TOIE0) | _BV(TOIE2);
	TCNT0 = 256 - (CPU_CLOCK_HZ / OS_TICKS_PER_SEC / 1024);
	OS_EXIT_CRITICAL();

	Mbox = OSMboxCreate(0);
	Sem = OSSemCreate(N_TASKS);
	MQueue = OSQCreate(MQueue_Table, MSG_QUEUE_SIZE);
	f_grp = OSFlagCreate(0x00, &err);

	OSTaskCreate(StageClearTask, (void *)0, (void *)&TaskStk[0][TASK_STK_SIZE - 1], 1);
	OSTaskCreate(AllClearTask, (void *)0, (void *)&TaskStk[1][TASK_STK_SIZE - 1], 2);
	OSTaskCreate(DeadTask, (void *)0, (void *)&TaskStk[2][TASK_STK_SIZE - 1], 3);
	OSTaskCreate(DisplayTask, (void *)0, (void *)&TaskStk[3][TASK_STK_SIZE - 1], 4);

	sei();

	OSStart();

	return 0;
}

ISR(TIMER2_OVF_vect) {
	if (sound != -1) {
		if (state == ON) {
			PORTB = 0x00;
			state = OFF;
		}
		else {
			PORTB = 0x10;
			state = ON;
		}
		TCNT2 = sound;
	}
}
ISR(INT4_vect){
	if (flag == 0) flag = 1;
	if (flag == 1) {
		if (curr != 0 && curr != meal) {
			curr--;
		}
	}
}
ISR(INT5_vect){
	if (flag == 1) {
		if (curr != 7 && curr != meal) {
			curr++;
		}
	}
}
void StageClearTask(void *data) {
	int i;
	INT8U err;
	char res;

	data = data;

	while (1) {
		res = (char*)OSMboxPend(Mbox, 0, &err);

		if (res == 1) {
			OSSemPend(Sem, 0, &err);
			PORTA = 0x00;
			for (i = 0; i < 300; i++) {
				PORTC = 0x5e;
				PORTG = FND_sel[0];
				_delay_us(2500);

				PORTC = FND_DATA[0];
				PORTG = FND_sel[1];
				_delay_us(2500);

				PORTC = FND_DATA[0];
				PORTG = FND_sel[2];
				_delay_us(2500);

				PORTC = 0x3d;
				PORTG = FND_sel[3];
				_delay_us(2500);

				if (i < 200)
					sound = clear[i / 20];
			}
			stage++;
			cnt = -1;
			curr = 7;
			meal = 7;
			flag = 0;
			OSSemPost(Sem);
		}
	}
}
void AllClearTask(void *data) {
	char res;
	INT8U err;
	int Time = 0;

	data = data;

	res = (char*)OSQPend(MQueue, 0, &err);

	while (res == 1) {
		OSSemPend(Sem, 0, &err);
		PORTA = 0x00;

		PORTC = 0x38;
		PORTG = FND_sel[0];
		_delay_us(2500);

		PORTC = 0x38;
		PORTG = FND_sel[1];
		_delay_us(2500);

		PORTC = 0x3e;
		PORTG = FND_sel[2];
		_delay_us(2500);

		PORTC = 0x71;
		PORTG = FND_sel[3];
		_delay_us(2500);

		if (Time < 1140) {
			Time++;
			sound = full[Time / 12];
		}
		OSSemPost(Sem);
	}
}
void DeadTask(void *data) {
	char res;
	INT8U err;
	int Time = 0;

	data = data;

	OSFlagPend(f_grp, 0xff, OS_FLAG_WAIT_SET_ALL + OS_FLAG_CONSUME, 0, &err);

	while (1) {
		OSSemPend(Sem, 0, &err);
		PORTA = 0x00;

		PORTC = 0x5e;
		PORTG = FND_sel[0];
		_delay_us(2500);

		PORTC = 0x77;
		PORTG = FND_sel[1];
		_delay_us(2500);

		PORTC = 0x79;
		PORTG = FND_sel[2];
		_delay_us(2500);

		PORTC = 0x5e;
		PORTG = FND_sel[3];
		_delay_us(2500);

		if (Time < 117) {
			Time++;
			sound = dead[Time / 9];
		}
		OSSemPost(Sem);
	}
}
void DisplayTask (void *data) {
	int i;
	int Time = 0;
	INT8U err;
    data = data;

	flag = 0;

    while(1)  {
		if (cnt >= 10) {
			OSMboxPost(Mbox, (void*)1);
		}
		if (stage == 3) {
			OSQPost(MQueue, (void*)1);
		}
		else {
			if (timer[stage] < 0) {
				OSFlagPost(f_grp, 0xff, OS_FLAG_SET, &err);
			}
			else {
				OSSemPend(Sem, 0, &err);
				PORTC = FND_DATA[(timer[stage] / 200) % 10];
				PORTG = FND_sel[0];
				_delay_us(1250);

				PORTC = FND_DATA[timer[stage] / 2000];
				PORTG = FND_sel[1];
				_delay_us(1250);

				PORTC = 0x40;
				PORTG = FND_sel[2];
				_delay_us(1250);

				PORTC = FND_DATA[cnt];
				PORTG = FND_sel[3];
				_delay_us(1250);

				if (flag == 1)
					timer[stage]--;

				if (flag == 1) {
					if ((timer[stage] / 100) % 2)
						PORTA = LED_sel[curr];
					else
						PORTA = (LED_sel[curr] | LED_sel[meal]);
				}
				else {
					PORTA = 0xff;
				}

				if (curr == meal) {
					if (cnt != -1) {
						for (i = 0; i < 3; i++) {
							sound = eat[i];
							_delay_ms(250);
						}
					}
					cnt++;
				}

				while (curr == meal)
					meal = rand() % 8;

				OSSemPost(Sem);
			}
		}
    }
}