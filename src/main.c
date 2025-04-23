#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hw.h"
#include "hw_tick.h"
#include "hw_usart.h"
#include "hw_can.h"
#include "car.h"
#include "canbox.h"
#include "conf.h"

static uint32_t rear_off_delay = 0;
static uint32_t rear_on_delay = 0;
static uint32_t rear_on_timeout = 200;
static void rear_delay_process(uint8_t ticks)
{
	uint8_t rear_state = (e_selector_r == car_get_selector()) ? 1 : 0;

	if (rear_state) {

		rear_on_delay += ticks;
		rear_off_delay = 0;
	}
	else {

		if (rear_on_delay && (rear_on_delay < 2 * rear_on_timeout))
			rear_off_delay = conf_get_rear_delay();

		rear_off_delay += ticks;
		rear_on_delay = 0;
	}
}

uint8_t get_rear_delay_state(void)
{
    uint8_t ign = car_get_ign();
    if (!ign) {
        // Reset delays if ignition goes off
        rear_on_delay = 0;
        rear_off_delay = MAX_REAR_DELAY; // Ensure it's considered OFF
        return 0;
    }

    if (car_get_selector() == e_selector_r) {
        rear_on_delay += 1; // Assuming called every tick (1ms) - adjust if using ticks argument
         if(rear_on_delay > rear_on_timeout * 2) rear_on_delay = rear_on_timeout * 2; // Prevent overflow
        rear_off_delay = 0;
    } else {
         // Start rear_off_delay ONLY if we were recently ON
        if (rear_on_delay >= rear_on_timeout) {
             rear_off_delay = 1; // Start the off delay immediately
        } else if (rear_off_delay > 0) { // Only increment if off-delay has started
            rear_off_delay += 1; // Increment off-delay
             if(rear_off_delay > conf_get_rear_delay() * 2) rear_off_delay = conf_get_rear_delay()*2; // Prevent overflow
        }
        rear_on_delay = 0; // Reset on-delay
    }


    // Determine final state based on delays
    if (rear_on_delay >= rear_on_timeout || (rear_off_delay > 0 && rear_off_delay < conf_get_rear_delay())) {
        return 1; // State is ON
    } else {
        return 0; // State is OFF
    }
}

struct key_cb_t key_cb =
{
	.mode = canbox_mode,
	.inc_volume = canbox_inc_volume,
	.dec_volume = canbox_dec_volume,
	.prev = canbox_prev,
	.next = canbox_next,
	.cont = canbox_cont,
	.navi = canbox_mode,
	.mici = canbox_mici,
};

uint8_t debug_on = 0;
uint32_t debug_on_cnt = 0;
uint8_t msg_idx = 0;

uint8_t sniffer_on = 0;

static void clr_screen(void)
{
	const char clr[] = "\033[2J";
	hw_usart_write(hw_usart_get(), (uint8_t *)clr, sizeof(clr));

	const char home[] = "\033[H";
	hw_usart_write(hw_usart_get(), (uint8_t *)home, sizeof(home));
}

static void clr_rscreen(void)
{
	const char clr[] = "\033[1J";
	hw_usart_write(hw_usart_get(), (uint8_t *)clr, sizeof(clr));

	const char home[] = "\033[H";
	hw_usart_write(hw_usart_get(), (uint8_t *)home, sizeof(home));
}

static void hide_cursor(void)
{
	const char hide[] = "\033[?25l";
	hw_usart_write(hw_usart_get(), (uint8_t *)hide, sizeof(hide));
}

static void print_line(void)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "--------------------\r\n");
	hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
}

static void debug_ch_process(uint8_t ch)
{
	if (ch == 'o') {

		debug_on = 0;
		sniffer_on = 0;

		clr_screen();
	}
	else if (ch == 'c') {

		enum e_car_t car = car_get_next_car();
		conf_set_car(car);
		car_init(conf_get_car(), &key_cb);
		hw_can_clr(hw_can_get_mscan());
	}
	else if (ch == 'b') {

		int cb = conf_get_canbox();
		cb++;
		if (cb >= e_cb_nums)
			cb = 0;

		conf_set_canbox(cb);
	}
	else if (ch == 'm') {

		uint8_t msgs_num = hw_can_get_msg_nums(hw_can_get_mscan());

		if (++msg_idx >= msgs_num)
			msg_idx = 0;
	}
	else if (ch == 's') {

		if (sniffer_on)
			sniffer_on = 0;
		else
			conf_write();
	}
	else if (ch == 'I') {

		uint8_t illum = conf_get_illum();
		illum++;
		if (illum >= 100)
			illum = 100;

		conf_set_illum(illum);
	}
	else if (ch == 'i') {

		uint8_t illum = conf_get_illum();
		illum--;
		if (illum <= 0 || illum >= 100)
			illum = 0;

		conf_set_illum(illum);
	}
	else if (ch == 'D') {

		uint16_t delay = conf_get_rear_delay();
		delay += 100;
		if (delay >= MAX_REAR_DELAY)
			delay = MAX_REAR_DELAY;

		conf_set_rear_delay(delay);
	}
	else if (ch == 'd') {

		uint16_t delay = conf_get_rear_delay();
		delay -= 100;
		if (delay <= 0 || delay >= MAX_REAR_DELAY)
			delay = 0;

		conf_set_rear_delay(delay);
	}
	else if (ch == 'S') {

		sniffer_on = 1;

		clr_rscreen();
	}
}

static void usart_process(void)
{
	uint8_t ch = 0;
	if (!hw_usart_read_ch(hw_usart_get(), &ch))
		return;

	if (!debug_on) {

		canbox_cmd_process(ch);

		if (ch == 'O') {

			if (debug_on_cnt++ > 10) {

				debug_on = 1;
				sniffer_on = 0;
				msg_idx = 0;

				clr_screen();
				hide_cursor();
			}
		}
		else
			debug_on_cnt = 0;
	}
	else
		debug_ch_process(ch);
}

extern uint32_t can_isr_cnt;

int max_show_sniff_num = 10;

void print_sniffer(void)
{
	char buf[200];

	clr_rscreen();

	snprintf(buf, sizeof(buf), "Sniffer window\r\n");
	hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

	print_line();

	uint8_t ids = hw_can_get_msg_nums(hw_can_get_mscan());
	uint32_t msgs = hw_can_get_pack_nums(hw_can_get_mscan());
	snprintf(buf, sizeof(buf), "Can: IDs:%" PRIu8 " Msgs:%" PRIu32 " Irqs:%" PRIu32 " \r\n", ids, msgs, can_isr_cnt);
	hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

	print_line();

	uint8_t nums = ids - msg_idx;
	if (nums > max_show_sniff_num)
		nums = max_show_sniff_num;

	for (int i = msg_idx; i < (msg_idx + nums); i++) {

		struct msg_can_t msg;
		if (!hw_can_get_msg(hw_can_get_mscan(), &msg, i))
			break;

		snprintf(buf, sizeof(buf), "0x%X : %02x %02x %02x %02x %02x %02x %02x %02x \r\n",
			(int)msg.id, msg.data[0], msg.data[1], msg.data[2], msg.data[3], msg.data[4], msg.data[5], msg.data[6], msg.data[7]);
		hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
	}

	print_line();

	snprintf(buf, sizeof(buf), "Ctrl keys: s - to main window; m - shift msgs\r\n");
	hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
}

uint32_t wakeups = 0;
//extern uint32_t usart_isr_cnt;

void print_debug(void)
{
    char buf[120]; // Increased buffer size slightly for longer lines

    // --- Retrieve ALL relevant states using getter functions ---
    enum e_car_t car = car_get_car();
    enum e_canbox_t cb = conf_get_canbox();
    uint8_t svin[18]; car_get_vin(svin); // Use uint8_t array for VIN
    uint32_t odo = car_get_odometer();

    uint8_t acc = car_get_acc();
    uint8_t ign = car_get_ign();
    uint8_t engine_running = car_get_engine(); // Get engine running status
    uint32_t rpm = car_get_taho(); // Get RPM
    uint16_t speed_kmh = car_get_speed(); // Get speed

    enum e_selector_t sel_enum = car_get_selector();
    uint8_t rear_active = get_rear_delay_state();
    int8_t wheel_angle = 0; car_get_wheel(&wheel_angle); // Assuming -100 to +100

    uint8_t illum_level = car_get_illum(); // This is likely the raw brightness level
    uint8_t park_lights = car_get_park_lights();
    uint8_t near_lights = car_get_near_lights();

    struct radar_t radar; car_get_radar(&radar);

    uint8_t fl_door = car_get_door_fl();
    uint8_t fr_door = car_get_door_fr();
    uint8_t rl_door = car_get_door_rl();
    uint8_t rr_door = car_get_door_rr();
    uint8_t bonnet = car_get_bonnet();
    uint8_t tailgate = car_get_tailgate();

    uint8_t park_brake = car_get_park_break();
    uint8_t low_washer = car_get_low_washer();
    uint8_t ds_belt = car_get_ds_belt(); // 1 = Unfastened

    uint32_t voltage_mv = car_get_voltage(); // Voltage likely in mV or scaled
    uint8_t low_voltage = car_get_low_voltage();
    int16_t temp_out = car_get_temp();
    uint8_t fuel_lvl_pct = car_get_fuel_level();
    uint8_t low_fuel = car_get_low_fuel_level();
    int16_t temp_coolant = car_get_engine_temp();
    int16_t temp_oil = car_get_oil_temp();

    // Trip Data
    uint16_t inst_cons_raw = car_get_inst_consumption_raw();
    uint16_t range_km = car_get_range_km();
    uint16_t avg_speed1 = car_get_avg_speed1();
    uint16_t avg_cons1_raw = car_get_avg_consumption1_raw();
    uint32_t trip_dist1 = car_get_trip_distance1(); // Note: 32-bit
    uint16_t avg_speed2 = car_get_avg_speed2();
    uint16_t avg_cons2_raw = car_get_avg_consumption2_raw();
    uint32_t trip_dist2 = car_get_trip_distance2(); // Note: 32-bit

    // Climate Data (Optional section)
    uint8_t ac_on = car_get_air_ac();
    uint8_t ac_max = car_get_air_ac_max();
    uint8_t recirc = car_get_air_recycling();
    uint8_t dual = car_get_air_dual();
    uint8_t rear_defrost = car_get_air_rear();
    uint8_t aqs = car_get_air_aqs();
    uint8_t auto_mode = car_get_air_auto_mode();

    uint8_t fan_speed = car_get_air_fanspeed();
    uint8_t air_wind = car_get_air_wind();
    uint8_t air_mid = car_get_air_middle();
    uint8_t air_floor = car_get_air_floor();
    uint8_t temp_l_raw = car_get_air_l_temp(); // Raw value from CAN
    uint8_t temp_r_raw = car_get_air_r_temp(); // Raw value from CAN

    uint8_t air_l_wind = car_get_air_l_wind();
    uint8_t air_l_mid = car_get_air_l_middle();
    uint8_t air_l_floor = car_get_air_l_floor();

    uint8_t air_r_wind = car_get_air_r_wind();
    uint8_t air_r_mid = car_get_air_r_middle();
    uint8_t air_r_floor = car_get_air_r_floor();

    // --- Clear Screen & Print Header ---
    clr_rscreen(); // Clear screen from cursor down

    snprintf(buf, sizeof(buf), "Main window\r\n");
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
    print_line();

    // --- Configuration Section ---
    snprintf(buf, sizeof(buf), "Configuration(%"PRIu16")\r\n", conf_get_idx()); // Use PRIu16 for uint16_t
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
    print_line();

    const char * scar = "";
    switch (car) { /* ... (keep existing car name logic) ... */
        case e_car_anymsg: scar = "Any Msg"; break;
        case e_car_lr2_2007my: scar = "LR2 2007MY"; break;
        case e_car_lr2_2013my: scar = "LR2 2013MY"; break;
        case e_car_peugeot_407: scar = "PEUGEOT 407 2008MY"; break;
        case e_car_xc90_2007my: scar = "XC90 2007MY"; break;
        case e_car_skoda_fabia: scar = "SKODA FABIA 2006MY"; break;
        case e_car_q3_2015: scar = "Audi Q3 2015"; break;
        case e_car_toyota_premio_26x: scar = "TOYOTA PREMIO 26x"; break;
        default: scar = "Unknown"; break;
    }

    const char * scb = "";
    switch (cb) { /* ... (keep existing canbox name logic) ... */
        case e_cb_raise_vw_pq: scb = "Raise VW(PQ)"; break;
        case e_cb_raise_vw_mqb: scb = "Raise VW(MQB)"; break;
        case e_cb_od_bmw_nbt_evo: scb = "Oudi BMW(NBT)"; break;
        case e_cb_hiworld_vw_mqb: scb = "HiWorld VW(MQB)"; break;
        case e_cb_hiworld_psa_pf2: scb = "Hiworld PSA(PF2)"; break;
        case e_cb_raise_psa_rcz: scb = "Raise RZC PSA"; break; // Added RZC
        default: scb = "Unknown"; break;
    }

    // Null-terminate VIN just in case car_get_vin didn't
    svin[sizeof(svin)-1] = '\0';
    snprintf(buf, sizeof(buf), "Car: %s Vin: %s Odo:%"PRIu32"km\r\n", scar, svin, odo);
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    snprintf(buf, sizeof(buf), "CanBox: %s\r\n", scb);
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    snprintf(buf, sizeof(buf), "Conf: Illum:%d Rear Delay:%d\r\n", conf_get_illum(), conf_get_rear_delay());
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    // --- State Section ---
    snprintf(buf, sizeof(buf), "State\r\n");
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
    print_line();

    snprintf(buf, sizeof(buf), "Sys : Uptime:%.5"PRIu32".%.3"PRIu32" Wakeups:%"PRIu32"\r\n", timer.sec, timer.msec % 1000, wakeups); // Corrected msec display
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    const char * ssel = "x"; // Selector string
    switch (sel_enum) { /* ... (keep existing selector logic) ... */
        case e_selector_p: ssel = "P"; break;
        case e_selector_r: ssel = "R"; break;
        case e_selector_n: ssel = "N"; break;
        case e_selector_d: ssel = "D"; break;
        case e_selector_m: ssel = "M"; break;
        case e_selector_m_p: ssel = "M+"; break;
        case e_selector_m_m: ssel = "M-"; break;
        case e_selector_s: ssel = "S"; break;
        default: ssel = "?"; break; // Handle unknown state
    }

    snprintf(buf, sizeof(buf), "Core: ACC:%d IGN:%d ENG:%d Gear:%s REAR:%d RPM:%"PRIu32"\r\n",
             acc, ign, engine_running, ssel, rear_active, rpm);
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    snprintf(buf, sizeof(buf), "Drv : Speed:%u km/h Wheel:%d%%\r\n", speed_kmh, wheel_angle); // Added speed
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    snprintf(buf, sizeof(buf), "Lght: Park:%d Near:%d Illum:%d\r\n", park_lights, near_lights, illum_level);
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    snprintf(buf, sizeof(buf), "Door: FL:%d FR:%d RL:%d RR:%d Bonnet:%d Tailgate:%d\r\n",
             fl_door, fr_door, rl_door, rr_door, bonnet, tailgate);
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    snprintf(buf, sizeof(buf), "Stat: PB:%d Belt:%d(1=Open) Washer:%d(1=Low)\r\n",
             park_brake, ds_belt, low_washer);
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    snprintf(buf, sizeof(buf), "Temp: Out:%d C Coolant:%d C Oil:%d C\r\n",
             temp_out, temp_coolant, temp_oil);
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    snprintf(buf, sizeof(buf), "Fuel: %3u%%(Low:%d) Volt:%.2fV(Low:%d)\r\n",
             fuel_lvl_pct, low_fuel, (float)voltage_mv / 100.0f, low_voltage); // Assuming voltage is mV*10
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    // Trip Computer Data (Using raw values - scaling interpretation depends on protocol)
    snprintf(buf, sizeof(buf), "Trip0: InstCons:%5u Range:%4u km\r\n", inst_cons_raw, range_km);
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
    snprintf(buf, sizeof(buf), "Trip1: AvgC:%5u AvgS:%3u Dst:%5"PRIu32"\r\n", avg_cons1_raw, avg_speed1, trip_dist1); // Use PRIu32 for distance
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
    snprintf(buf, sizeof(buf), "Trip2: AvgC:%5u AvgS:%3u Dst:%5"PRIu32"\r\n", avg_cons2_raw, avg_speed2, trip_dist2); // Use PRIu32 for distance
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));


    // --- Climate Section (Optional - Add if useful) ---
    snprintf(buf, sizeof(buf), "AC  : On:%d Max:%d Rec:%d Dual:%d Rear:%d AQS:%d Fan:%d Auto:%d\r\n",
             ac_on, ac_max, recirc, dual, rear_defrost, aqs, fan_speed, auto_mode);
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
    snprintf(buf, sizeof(buf), "Air : Wind:%d Mid:%d Floor:%d | Temp L:%d R:%d \r\n",
             air_wind, air_mid, air_floor, temp_l_raw, temp_r_raw);          
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    snprintf(buf, sizeof(buf), "AirL: Wind:%d Mid:%d Floor:%d | AirR: Wind:%d Mid:%d Floor:%d\r\n",
             air_l_wind, air_l_mid, air_l_floor, air_r_wind, air_r_mid, air_r_floor);
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    // Add seat heating if needed: LSeat:%d RSeat:%d

    // --- Radar Section ---
    snprintf(buf, sizeof(buf), "Radar St:%X | F:%d %d %d %d | R:%d %d %d %d (0-7)\r\n",
             radar.state,
             radar.fl, radar.flm, radar.frm, radar.fr,
             radar.rl, radar.rlm, radar.rrm, radar.rr);
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    // --- CAN Stats/Message Section ---
    uint8_t n1 = hw_can_get_msg_nums(hw_can_get_mscan());
    uint32_t n2 = hw_can_get_pack_nums(hw_can_get_mscan());
    snprintf(buf, sizeof(buf), "CAN : IDs:%"PRIu8" Msgs:%"PRIu32" Irqs:%"PRIu32"\r\n", n1, n2, can_isr_cnt); // Use PRIu8/PRIu32
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));

    uint8_t msgs_num = hw_can_get_msg_nums(hw_can_get_mscan());
    if (msgs_num > 0 && msg_idx < msgs_num) {
        struct msg_can_t msg;
        if (hw_can_get_msg(hw_can_get_mscan(), &msg, msg_idx)) {
            snprintf(buf, sizeof(buf), "Msg%d/%d:%03X#%02X%02X%02X%02X%02X%02X%02X%02X\r\n", // Compact hex format
                     msg_idx + 1, msgs_num, (unsigned int)msg.id, // Cast id to unsigned int for %X
                     msg.data[0], msg.data[1], msg.data[2], msg.data[3],
                     msg.data[4], msg.data[5], msg.data[6], msg.data[7]);
            hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
        }
    } else if (msgs_num == 0) {
         snprintf(buf, sizeof(buf), "Msg -/-: No messages received yet.\r\n");
         hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
    }

    // --- Footer / Commands ---
    print_line();
    snprintf(buf, sizeof(buf), "Keys: o:Exit m:NxtMsg s:Save S:Sniff c:Car b:Box i/I:Illum d/D:Delay\r\n");
    hw_usart_write(hw_usart_get(), (uint8_t *)buf, strlen(buf));
}

static void gpio_process(void)
{
    uint8_t acc = car_get_acc();
    // uint8_t ign = car_get_ign(); // IGN state not directly needed for GPIO here

    // --- Illumination Pin Control ---
    // Directly use the last known state from the 0x036 handler
    bool lights_pin_on = car_get_illum();

    // Optional: Also require ACC to be ON for the ILL pin? Typically yes for head units.
    lights_pin_on = lights_pin_on && acc;

    if (acc)
        hw_gpio_acc_on();
    else
        hw_gpio_acc_off();

    if (lights_pin_on)
        hw_gpio_ill_on();
    else
        hw_gpio_ill_off();

    if (get_rear_delay_state())
        hw_gpio_rear_on();
    else
        hw_gpio_rear_off();

    // --- Park Pin Control ---
    // (Assuming PARK signal logic is Ground when ON, High impedance when OFF)
    uint8_t park_brake = car_get_park_break(); // Needs handler for the park brake CAN message
    if (park_brake == 1) { // If Park Brake is ON
        hw_gpio_park_on(); // Set pin LOW (Ground)
    } else {
        hw_gpio_park_off(); // Set pin HIGH/High-Z
    }
}

uint8_t fmax_global[4] = { 10, 10, 10, 10 }; // Example global fmax/rmax, adjust as needed
uint8_t rmax_global[4] = { 10, 10, 10, 10 };


int main(void)
{
	hw_setup();

	conf_read();

	car_init(conf_get_car(), &key_cb);

	uint8_t acc = car_get_acc();
	uint32_t ms_can_nums = 0;
	uint32_t ms_can_stop_counter = 0;

	while(1) {

		gpio_process();
		usart_process();

		if (timer.flag_tick) {

			timer.flag_tick = 0;

			rear_delay_process(1);
		}

		if (timer.flag_5ms) {

			timer.flag_5ms = 0;

			car_process(5);
		}

		if (timer.flag_100ms) {

			timer.flag_100ms = 0;

			if (!debug_on)
				canbox_park_process(fmax_global, rmax_global); // Call park process with fmax/rmax
		}

		if (timer.flag_250ms) {

			timer.flag_250ms = 0;

			if (debug_on) {

				if (sniffer_on)
					print_sniffer();
				else
					print_debug();
			}
			else {

				canbox_process();
			}
		}
#ifdef DEBUG
		if (timer.flag_1000ms) {

			timer.flag_1000ms = 0;

			debug_on_cnt = 0;

			uint32_t nums = hw_can_get_pack_nums(hw_can_get_mscan());
			if (nums == ms_can_nums)
				ms_can_stop_counter++;
			else
				ms_can_stop_counter = 0;

			if (acc && (conf_get_car() == e_car_skoda_fabia))
				ms_can_stop_counter = 1;

			ms_can_nums = nums;

			if (ms_can_stop_counter > 10) {

				conf_write();

				hw_sleep();

				ms_can_stop_counter = 0;

				hw_setup();

				conf_read();

				debug_on = 0;

				wakeups++;
			}
		}
#endif
	}
}
