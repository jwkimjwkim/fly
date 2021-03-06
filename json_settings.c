/*******************************************************************************
* json_settings.c
*
* contains all the functions for io to the settings file, including default
* values that can be written to disk if no file is present.
*******************************************************************************/

#include <json-c/json.h>
#include <stdio.h>
#include <string.h>	// FOR str_cmp()
#include <fcntl.h>	// for F_OK
#include <unistd.h> 	// for access()

#include <roboticscape.h>
#include "fly_defs.h"
#include "fly_types.h"

// local copy of controllers to store before feedback_controller.c requests it
rc_filter_t roll_controller;
rc_filter_t pitch_controller;
rc_filter_t yaw_controller;
rc_filter_t altitude_controller;

// if anything goes wrong set this flag back to 0
int was_load_successful = 0;

// local functions
json_object* get_default_settings();
int print_settings(json_object* jobj);
int write_settings_to_file(json_object* jobj);
int parse_flight_mode(json_object* jobj, flight_mode_t* mode);
int parse_controller(json_object* jobj, rc_filter_t* filter, int sample_rate_hz);

/*******************************************************************************
* int load_settings_from_file(fly_settings_t* settings)
*
* Populates the settings struct with the json file contents.
* If no settings file exists, it makes a new one filled with defaults.
* Also loads in controllers from file but stores them locally and they can be
* retrieved with 
*******************************************************************************/
int load_settings_from_file(fly_settings_t* settings){

	roll_controller = rc_empty_filter();	
	#ifdef DEBUG
	printf("debug\n");
	#else
	printf("no debug\n");
	#endif

	#ifdef DEBUG
	//rc_usleep(500000);
	fprintf(stderr,"beginning of load_settings_from_file\n");
	rc_usleep(500000);
	#endif
	struct json_object *jobj = NULL;	// holds the top level obj from file
	struct json_object *tmp = NULL;		// temp object
	char* tmp_str = NULL; // temp string poitner
	double tmp_flt;
	int tmp_int;

	was_load_successful = 0; 

	#ifdef DEBUG
	printf("about to check access of fly settings file\n");
	#endif
	if(access(FLY_SETTINGS_FILE, F_OK)!=0){
		printf("Fly settings file missing, making default\n");
		jobj = get_default_settings();
		printf("Writing default settings to file\n");
		write_settings_to_file(jobj);
	}
	else{
		#ifdef DEBUG
		printf("about to read json from file\n");
		#endif
		jobj = json_object_from_file(FLY_SETTINGS_FILE);
		if(jobj==NULL){
			printf("ERROR, failed to read settings from disk\n");
			return -1;
		}
	}

	#ifdef DEBUG
	print_settings(jobj);
	#endif

	// parse layout
	if(json_object_object_get_ex(jobj, "layout", &tmp)==0){
		printf("ERROR: can't find layout in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_string)==0){
		printf("ERROR: layout should be a string\n");
		return -1;
	}
	tmp_str = (char*)json_object_get_string(tmp);
	if(strcmp(tmp_str, "LAYOUT_6DOF_ROTORBITS") == 0){
		settings->num_rotors = 6;
		settings->layout = LAYOUT_6DOF_ROTORBITS;
	}
	else if(strcmp(tmp_str, "LAYOUT_4X") == 0){
		settings->num_rotors = 4;
		settings->layout = LAYOUT_4X;
	}
	else if(strcmp(tmp_str, "LAYOUT_4PLUS") == 0){
		settings->num_rotors = 4;
		settings->layout = LAYOUT_4PLUS;
	}
	else if(strcmp(tmp_str, "LAYOUT_6X") == 0){
		settings->num_rotors = 6;
		settings->layout = LAYOUT_6X;
	}
	else if(strcmp(tmp_str, "LAYOUT_8X") == 0){
		settings->num_rotors = 8;
		settings->layout = LAYOUT_8X;
	}
	else{
		printf("ERROR: invalid layout string\n");
		return -1;
	}


	// parse thrust map
	if(json_object_object_get_ex(jobj, "thrust_map", &tmp)==0){
		printf("ERROR: can't find thrust_map in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_string)==0){
		printf("ERROR: thrust map should be a string\n");
		return -1;
	}
	tmp_str = (char*)json_object_get_string(tmp);
	if(strcmp(tmp_str, "MN1806_1400KV_4S") == 0){
		settings->thrust_map = MN1806_1400KV_4S;
	}
	else if(strcmp(tmp_str, "F20_2300KV_2S") == 0){
		settings->thrust_map = F20_2300KV_2S;
	}
	else{
		printf("ERROR: invalid thrust_map string\n");
		return -1;
	}


	// parse bbb_orientation
	if(json_object_object_get_ex(jobj, "bbb_orientation", &tmp)==0){
		printf("ERROR: can't find bbb_orientation in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_string)==0){
		printf("ERROR: bbb_orientation should be a string\n");
		return -1;
	}
	tmp_str = (char*)json_object_get_string(tmp);
	if(strcmp(tmp_str, "ORIENTATION_X_FORWARD") == 0){
		settings->bbb_orientation = ORIENTATION_X_FORWARD;
	}
	else if(strcmp(tmp_str, "ORIENTATION_Z_UP") == 0){
		settings->bbb_orientation = ORIENTATION_Z_UP;
	}
	else{
		printf("ERROR: invalid BBB Orientation\n");
		return -1;
	}


	// parse v_nominal
	if(json_object_object_get_ex(jobj, "v_nominal", &tmp)==0){
		printf("ERROR: can't find v_nominal in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_double)==0){
		printf("ERROR: v_nominal should be a double\n");
		return -1;
	}
	tmp_flt = json_object_get_double(tmp);
	if(tmp_flt<7.0 || tmp_flt>18){
		printf("ERROR: v_nominal should be between 7 and 18\n");
		return -1;
	}
	settings->v_nominal = tmp_flt;


	// parse enable_freefall_detect
	if(json_object_object_get_ex(jobj, "enable_freefall_detect", &tmp)==0){
		printf("ERROR: can't find enable_freefall_detect in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_boolean)==0){
		printf("ERROR: enable_freefall_detect should be a boolean\n");
		return -1;
	}
	settings->enable_freefall_detect = json_object_get_boolean(tmp);


	// parse enable_logging
	if(json_object_object_get_ex(jobj, "enable_logging", &tmp)==0){
		printf("ERROR: can't find enable_logging in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_boolean)==0){
		printf("ERROR: enable_logging should be a boolean\n");
		return -1;
	}
	settings->enable_logging = json_object_get_boolean(tmp);



	// parse flight_mode_1
	if(json_object_object_get_ex(jobj, "flight_mode_1", &tmp)==0){
		printf("ERROR: can't find flight_mode_1 in settings file\n");
		return -1;
	}
	if(parse_flight_mode(tmp, &settings->flight_mode_1)) return -1;

	// parse flight_mode_2
	if(json_object_object_get_ex(jobj, "flight_mode_2", &tmp)==0){
		printf("ERROR: can't find flight_mode_2 in settings file\n");
		return -1;
	}
	if(parse_flight_mode(tmp, &settings->flight_mode_2)) return -1;

	// parse flight_mode_3
	if(json_object_object_get_ex(jobj, "flight_mode_3", &tmp)==0){
		printf("ERROR: can't find flight_mode_3 in settings file\n");
		return -1;
	}
	if(parse_flight_mode(tmp, &settings->flight_mode_3)) return -1;


	// parse num_dsm_modes
	if(json_object_object_get_ex(jobj, "num_dsm_modes", &tmp)==0){
		printf("ERROR: can't find num_dsm_modes in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: num_dsm_modes should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int<1 || tmp_int>3){
		printf("ERROR: num_dsm_modes must be 1,2 or 3\n");
		return -1;
	}
	settings->num_dsm_modes = tmp_int;


	// parse dsm_thr_ch
	if(json_object_object_get_ex(jobj, "dsm_thr_ch", &tmp)==0){
		printf("ERROR: can't find dsm_thr_ch in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: dsm_thr_ch should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int<1 || tmp_int>9){
		printf("ERROR: DSM channel must be between 1 & 9\n");
		return -1;
	}
	settings->dsm_thr_ch = tmp_int;


	// parse dsm_roll_ch
	if(json_object_object_get_ex(jobj, "dsm_roll_ch", &tmp)==0){
		printf("ERROR: can't find dsm_roll_ch in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: dsm_roll_ch should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int<1 || tmp_int>9){
		printf("ERROR: DSM channel must be between 1 & 9\n");
		return -1;
	}
	settings->dsm_roll_ch = tmp_int;


	// parse dsm_pitch_ch
	if(json_object_object_get_ex(jobj, "dsm_pitch_ch", &tmp)==0){
		printf("ERROR: can't find dsm_pitch_ch in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int<1 || tmp_int>9){
		printf("ERROR: DSM channel must be between 1 & 9\n");
		return -1;
	}
	settings->dsm_pitch_ch = tmp_int;


	// parse dsm_yaw_ch
	if(json_object_object_get_ex(jobj, "dsm_yaw_ch", &tmp)==0){
		printf("ERROR: can't find parse dsm_yaw_ch in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: parse dsm_yaw_ch should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int<1 || tmp_int>9){
		printf("ERROR: DSM channel must be between 1 & 9\n");
		return -1;
	}
	settings->dsm_yaw_ch = tmp_int;


	// parse dsm_mode_ch
	if(json_object_object_get_ex(jobj, "dsm_mode_ch", &tmp)==0){
		printf("ERROR: can't find dsm_mode_ch in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: dsm_mode_ch should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int<1 || tmp_int>9){
		printf("ERROR: DSM channel must be between 1 & 9\n");
		return -1;
	}
	settings->dsm_mode_ch = tmp_int;


	// parse dsm_kill_ch
	if(json_object_object_get_ex(jobj, "dsm_kill_ch", &tmp)==0){
		printf("ERROR: can't find dsm_kill_ch in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: dsm_kill_ch should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int<1 || tmp_int>9){
		printf("ERROR: DSM channel must be between 1 & 9\n");
		return -1;
	}
	settings->dsm_kill_ch = tmp_int;






	// parse dsm_thr_pol
	if(json_object_object_get_ex(jobj, "dsm_thr_pol", &tmp)==0){
		printf("ERROR: can't find dsm_thr_pol in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: dsm_thr_pol should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int!=1 && tmp_int!=-1){
		printf("ERROR: DSM channel polarity must be 1 or -1\n");
		return -1;
	}
	settings->dsm_thr_pol = tmp_int;


	// parse dsm_roll_pol
	if(json_object_object_get_ex(jobj, "dsm_roll_pol", &tmp)==0){
		printf("ERROR: can't find dsm_roll_pol in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: dsm_roll_pol should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int!=1 && tmp_int!=-1){
		printf("ERROR: DSM channel polarity must be 1 or -1\n");
		return -1;
	}
	settings->dsm_roll_pol = tmp_int;


	// parse dsm_pitch_pol
	if(json_object_object_get_ex(jobj, "dsm_pitch_pol", &tmp)==0){
		printf("ERROR: can't find dsm_pitch_pol in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int!=1 && tmp_int!=-1){
		printf("ERROR: DSM channel polarity must be 1 or -1\n");
		return -1;
	}
	settings->dsm_pitch_pol = tmp_int;


	// parse dsm_yaw_pol
	if(json_object_object_get_ex(jobj, "dsm_yaw_pol", &tmp)==0){
		printf("ERROR: can't find parse dsm_yaw_pol in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: parse dsm_yaw_pol should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int!=1 && tmp_int!=-1){
		printf("ERROR: DSM channel polarity must be 1 or -1\n");
		return -1;
	}
	settings->dsm_yaw_pol = tmp_int;


	// parse dsm_mode_pol
	if(json_object_object_get_ex(jobj, "dsm_mode_pol", &tmp)==0){
		printf("ERROR: can't find dsm_mode_pol in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: dsm_mode_pol should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int!=1 && tmp_int!=-1){
		printf("ERROR: DSM channel polarity must be 1 or -1\n");
		return -1;
	}
	settings->dsm_mode_pol = tmp_int;


	// parse dsm_kill_pol
	if(json_object_object_get_ex(jobj, "dsm_kill_pol", &tmp)==0){
		printf("ERROR: can't find dsm_kill_pol in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: dsm_kill_pol should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int!=1 && tmp_int!=-1){
		printf("ERROR: DSM channel polarity must be 1 or -1\n");
		return -1;
	}
	settings->dsm_kill_pol = tmp_int;


	// parse feedback_hz
	if(json_object_object_get_ex(jobj, "feedback_hz", &tmp)==0){
		printf("ERROR: can't find feedback_hz in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_int)==0){
		printf("ERROR: feedback_hz should be an int\n");
		return -1;
	}
	tmp_int = json_object_get_int(tmp);
	if(tmp_int!=50 && tmp_int!=100 && tmp_int!=200){
		printf("ERROR: feedback_hz must be 50,100,or 200\n");
		return -1;
	}
	settings->feedback_hz = tmp_int;
	fprintf(stderr, "hz in json_settings: %d\n", tmp_int);



	// parse printf_arm
	if(json_object_object_get_ex(jobj, "printf_arm", &tmp)==0){
		printf("ERROR: can't find printf_arm in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_boolean)==0){
		printf("ERROR: printf_arm should be a boolean\n");
		return -1;
	}
	settings->printf_arm = json_object_get_boolean(tmp);

	// parse printf_altitude
	if(json_object_object_get_ex(jobj, "printf_altitude", &tmp)==0){
		printf("ERROR: can't find printf_altitude in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_boolean)==0){
		printf("ERROR: printf_altitude should be a boolean\n");
		return -1;
	}
	settings->printf_altitude = json_object_get_boolean(tmp);


	// parse printf_rpy
	if(json_object_object_get_ex(jobj, "printf_rpy", &tmp)==0){
		printf("ERROR: can't find printf_rpy in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_boolean)==0){
		printf("ERROR: printf_rpy should be a boolean\n");
		return -1;
	}
	settings->printf_rpy = json_object_get_boolean(tmp);


	// parse printf_sticks
	if(json_object_object_get_ex(jobj, "printf_sticks", &tmp)==0){
		printf("ERROR: can't find printf_sticks in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_boolean)==0){
		printf("ERROR: printf_sticks should be a boolean\n");
		return -1;
	}
	settings->printf_sticks = json_object_get_boolean(tmp);

	// parse printf_setpoint
	if(json_object_object_get_ex(jobj, "printf_setpoint", &tmp)==0){
		printf("ERROR: can't find printf_setpoint in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_boolean)==0){
		printf("ERROR: printf_setpoint should be a boolean\n");
		return -1;
	}
	settings->printf_setpoint = json_object_get_boolean(tmp);

	// parse printf_u
	if(json_object_object_get_ex(jobj, "printf_u", &tmp)==0){
		printf("ERROR: can't find printf_u in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_boolean)==0){
		printf("ERROR: printf_u should be a boolean\n");
		return -1;
	}
	settings->printf_u = json_object_get_boolean(tmp);


	// parse printf_motors
	if(json_object_object_get_ex(jobj, "printf_motors", &tmp)==0){
		printf("ERROR: can't find printf_motors in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_boolean)==0){
		printf("ERROR: printf_motors should be a boolean\n");
		return -1;
	}
	settings->printf_motors = json_object_get_boolean(tmp);


	// parse printf_mode
	if(json_object_object_get_ex(jobj, "printf_mode", &tmp)==0){
		printf("ERROR: can't find printf_mode in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_boolean)==0){
		printf("ERROR: printf_mode should be a boolean\n");
		return -1;
	}
	settings->printf_mode = json_object_get_boolean(tmp);



	// parse roll controller 
	if(json_object_object_get_ex(jobj, "roll_controller", &tmp)==0){
		printf("ERROR: can't find roll_controller in settings file\n");
		return -1;
	}
	if(parse_controller(tmp, &roll_controller, settings->feedback_hz)){
		printf("ERROR: could not parse roll_controller\n");
		return -1;
	}

	// parse pitch controller
	if(json_object_object_get_ex(jobj, "pitch_controller", &tmp)==0){
		printf("ERROR: can't find pitch_controller in settings file\n");
		return -1;
	}
	if(parse_controller(tmp, &pitch_controller, settings->feedback_hz)){
		printf("ERROR: could not parse pitch_controller\n");
		return -1;
	}

	// parse yaw controller
	if(json_object_object_get_ex(jobj, "yaw_controller", &tmp)==0){
		printf("ERROR: can't find yaw_controller in settings file\n");
		return -1;
	}
	if(parse_controller(tmp, &yaw_controller, settings->feedback_hz)){
		printf("ERROR: could not parse yaw_controller\n");
		return -1;
	}

	// parse altitude controller 
	if(json_object_object_get_ex(jobj, "altitude_controller", &tmp)==0){
		printf("ERROR: can't find altitude_controller in settings file\n");
		return -1;
	}
	if(parse_controller(tmp, &altitude_controller, settings->feedback_hz)){
		printf("ERROR: could not parse altitude_controller\n");
		return -1;
	}



	json_object_put(jobj);	// free memory
	was_load_successful = 1;
	return 0;
}



/*******************************************************************************
* json_object* get_default_settings()
*
* returns default settings in json format
*******************************************************************************/
json_object* get_default_settings(){
	struct json_object *out = NULL;		// json object to be returned
	struct json_object *array = NULL;	// temp object for new arrays
	struct json_object *tmp = NULL;		// temp object
	struct json_object *tmp2 = NULL;	// temp object

	// make new object to return
	out = json_object_new_object();

	// Physical Parameters
	tmp = json_object_new_string("LAYOUT_6DOF_ROTORBITS");
	json_object_object_add(out, "layout", tmp);
	tmp = json_object_new_string("MN1806_1400KV_4S");
	json_object_object_add(out, "thrust_map", tmp);
	tmp = json_object_new_string("ORIENTATION_X_FORWARD");
	json_object_object_add(out, "bbb_orientation", tmp);
	tmp = json_object_new_double(7.4);
	json_object_object_add(out, "v_nominal", tmp);
	// feedback loop frequency
	tmp = json_object_new_int(100);
	json_object_object_add(out, "feedback_hz", tmp);

	// features
	tmp = json_object_new_boolean(FALSE);
	json_object_object_add(out, "enable_freefall_detect", tmp);
	tmp = json_object_new_boolean(FALSE);
	json_object_object_add(out, "enable_logging", tmp);

	// flight modes
	tmp = json_object_new_string("DIRECT_THROTTLE");
	json_object_object_add(out, "flight_mode_1", tmp);
	tmp = json_object_new_string("DIRECT_THROTTLE");
	json_object_object_add(out, "flight_mode_2", tmp);
	tmp = json_object_new_string("DIRECT_THROTTLE");
	json_object_object_add(out, "flight_mode_3", tmp);

	// DSM radio config
	tmp = json_object_new_int(1);
	json_object_object_add(out, "dsm_thr_ch", tmp);
	tmp = json_object_new_int(2);
	json_object_object_add(out, "dsm_roll_ch", tmp);
	tmp = json_object_new_int(3);
	json_object_object_add(out, "dsm_pitch_ch", tmp);
	tmp = json_object_new_int(4);
	json_object_object_add(out, "dsm_yaw_ch", tmp);
	tmp = json_object_new_int(5);
	json_object_object_add(out, "dsm_mode_ch", tmp);
	tmp = json_object_new_int(6);
	json_object_object_add(out, "dsm_kill_ch", tmp);
	tmp = json_object_new_int(1);
	json_object_object_add(out, "dsm_thr_pol", tmp);
	tmp = json_object_new_int(1);
	json_object_object_add(out, "dsm_roll_pol", tmp);
	tmp = json_object_new_int(1);
	json_object_object_add(out, "dsm_pitch_pol", tmp);
	tmp = json_object_new_int(1);
	json_object_object_add(out, "dsm_yaw_pol", tmp);
	tmp = json_object_new_int(1);
	json_object_object_add(out, "dsm_mode_pol", tmp);
	tmp = json_object_new_int(1);
	json_object_object_add(out, "dsm_kill_pol", tmp);
	tmp = json_object_new_int(3);
	json_object_object_add(out, "num_dsm_modes", tmp);

	// features
	tmp = json_object_new_boolean(TRUE);
	json_object_object_add(out, "printf_arm", tmp);
	tmp = json_object_new_boolean(FALSE);
	json_object_object_add(out, "printf_altitude", tmp);
	tmp = json_object_new_boolean(FALSE);
	json_object_object_add(out, "printf_rpy", tmp);
	tmp = json_object_new_boolean(FALSE);
	json_object_object_add(out, "printf_sticks", tmp);
	tmp = json_object_new_boolean(TRUE);
	json_object_object_add(out, "printf_setpoint", tmp);
	tmp = json_object_new_boolean(FALSE);
	json_object_object_add(out, "printf_u", tmp);
	tmp = json_object_new_boolean(FALSE);
	json_object_object_add(out, "printf_motors", tmp);
	tmp = json_object_new_boolean(TRUE);
	json_object_object_add(out, "printf_mode", tmp);


	// roll controller
	tmp2 = json_object_new_object();
	tmp = json_object_new_double(1);
	json_object_object_add(tmp2, "gain", tmp);
	tmp = json_object_new_string("CT");
	json_object_object_add(tmp2, "CT_or_DT", tmp);
	tmp = json_object_new_double(6.283);
	json_object_object_add(tmp2, "crossover_freq_rad_per_sec", tmp);

	array = json_object_new_array();
	tmp = json_object_new_double(0.1);
	json_object_array_add(array, tmp);
	tmp = json_object_new_double(0.2);
	json_object_array_put_idx(array, 1, tmp);
	tmp = json_object_new_double(0.3);
	json_object_array_put_idx(array, 2, tmp);
	json_object_object_add(tmp2, "numerator", array);

	array = json_object_new_array();
	tmp = json_object_new_double(0.1);
	json_object_array_add(array, tmp);
	tmp = json_object_new_double(0.2);
	json_object_array_put_idx(array, 1, tmp);
	tmp = json_object_new_double(0.3);
	json_object_array_put_idx(array, 2, tmp);
	json_object_object_add(tmp2, "denominator", array);
	
	json_object_object_add(out, "roll_controller", tmp2);

	// pitch controller
	tmp2 = json_object_new_object();
	tmp = json_object_new_double(1.0);
	json_object_object_add(tmp2, "gain", tmp);
	tmp = json_object_new_string("CT");
	json_object_object_add(tmp2, "CT_or_DT", tmp);
	tmp = json_object_new_double(6.283);
	json_object_object_add(tmp2, "crossover_freq_rad_per_sec", tmp);

	array = json_object_new_array();
	tmp = json_object_new_double(0.1);
	json_object_array_add(array, tmp);
	tmp = json_object_new_double(0.2);
	json_object_array_put_idx(array, 1, tmp);
	tmp = json_object_new_double(0.3);
	json_object_array_put_idx(array, 2, tmp);
	json_object_object_add(tmp2, "numerator", array);

	array = json_object_new_array();
	tmp = json_object_new_double(0.1);
	json_object_array_add(array, tmp);
	tmp = json_object_new_double(0.2);
	json_object_array_put_idx(array, 1, tmp);
	tmp = json_object_new_double(0.3);
	json_object_array_put_idx(array, 2, tmp);
	json_object_object_add(tmp2, "denominator", array);
	
	json_object_object_add(out, "pitch_controller", tmp2);

	// yaw controller
	tmp2 = json_object_new_object();
	tmp = json_object_new_double(1.0);
	json_object_object_add(tmp2, "gain", tmp);
	tmp = json_object_new_string("CT");
	json_object_object_add(tmp2, "CT_or_DT", tmp);
	tmp = json_object_new_double(3.141);
	json_object_object_add(tmp2, "crossover_freq_rad_per_sec", tmp);

	array = json_object_new_array();
	tmp = json_object_new_double(0.1);
	json_object_array_add(array, tmp);
	tmp = json_object_new_double(0.2);
	json_object_array_put_idx(array, 1, tmp);
	tmp = json_object_new_double(0.3);
	json_object_array_put_idx(array, 2, tmp);
	json_object_object_add(tmp2, "numerator", array);

	array = json_object_new_array();
	tmp = json_object_new_double(0.1);
	json_object_array_add(array, tmp);
	tmp = json_object_new_double(0.2);
	json_object_array_put_idx(array, 1, tmp);
	tmp = json_object_new_double(0.3);
	json_object_array_put_idx(array, 2, tmp);
	json_object_object_add(tmp2, "denominator", array);
	
	json_object_object_add(out, "yaw_controller", tmp2);

	// altitude controller
	tmp2 = json_object_new_object();
	tmp = json_object_new_double(1.0);
	json_object_object_add(tmp2, "gain", tmp);
	tmp = json_object_new_string("CT");
	json_object_object_add(tmp2, "CT_or_DT", tmp);
	tmp = json_object_new_double(0.6283);
	json_object_object_add(tmp2, "crossover_freq_rad_per_sec", tmp);

	array = json_object_new_array();
	tmp = json_object_new_double(0.1);
	json_object_array_add(array, tmp);
	tmp = json_object_new_double(0.2);
	json_object_array_put_idx(array, 1, tmp);
	tmp = json_object_new_double(0.3);
	json_object_array_put_idx(array, 2, tmp);
	json_object_object_add(tmp2, "numerator", array);

	array = json_object_new_array();
	tmp = json_object_new_double(0.1);
	json_object_array_add(array, tmp);
	tmp = json_object_new_double(0.2);
	json_object_array_put_idx(array, 1, tmp);
	tmp = json_object_new_double(0.3);
	json_object_array_put_idx(array, 2, tmp);
	json_object_object_add(tmp2, "denominator", array);
	
	json_object_object_add(out, "altitude_controller", tmp2);

	return out;
}


/*******************************************************************************
* int print_settings(json_object* jobj)
*
* only used in debug mode
*******************************************************************************/
int print_settings(json_object* jobj){
	if(jobj == NULL){
		printf("ERROR: NULL object passed to print_settings\n");
		return -1;
	}
	printf("settings:\n\n");
	printf("%s", json_object_to_json_string_ext(jobj, \
							JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));
	printf("\n");
	return 0;
}

/*******************************************************************************
* int write_settings_to_file(json_object* jobj)
*
* wrapper to clean up writing to file
*******************************************************************************/
int write_settings_to_file(json_object* jobj){
	int out;
	out = json_object_to_file_ext(FLY_SETTINGS_FILE, jobj, \
		JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY);
	//out = json_object_to_file(FLY_SETTINGS_FILE, jobj);
	printf("just wrote settings to file\n");
	return out;
}


/*******************************************************************************
* int parse_flight_mode(json_object* jobj, flight_mode_t* mode)
*
* parses a json_object and fills in the flight mode.
* returns 0 on success or -1 on failure
*******************************************************************************/
int parse_flight_mode(json_object* jobj, flight_mode_t* mode){
	char* tmp_str = NULL;

	if(json_object_is_type(jobj, json_type_string)==0){
		printf("ERROR: flight_mode should be a string\n");
		return -1;
	}
	tmp_str = (char*)json_object_get_string(jobj);
	if(strcmp(tmp_str, "DIRECT_THROTTLE") == 0){
		*mode = DIRECT_THROTTLE;
	}
	else if(strcmp(tmp_str, "FALLBACK_4DOF") == 0){
		*mode = FALLBACK_4DOF;
	}
	else if(strcmp(tmp_str, "TEST_BENCH") == 0){
		*mode = TEST_BENCH;
	}
	else{
		printf("ERROR: invalid flight mode\n");
		return -1;
	}
	return 0;
}


/*******************************************************************************
* int parse_controller(json_object* jobj, rc_filter_t* filter, int feedback_hz)
*
* parses a json_object and sets up a new controller
* returns 0 on success or -1 on failure
*******************************************************************************/
int parse_controller(json_object* jobj, rc_filter_t* filter, int feedback_hz){
	struct json_object *array = NULL;	// to hold num & den arrays
	struct json_object *tmp = NULL;		// temp object
	char* tmp_str = NULL;
	double tmp_flt;
	int i, num_len, den_len;
	rc_vector_t num_vec = rc_empty_vector();
	rc_vector_t den_vec = rc_empty_vector();
	double dt = 1.0/feedback_hz;

	// destroy old memory in case the order changes
	rc_free_filter(filter);

	// pull out gain
	if(json_object_object_get_ex(jobj, "gain", &tmp)==0){
		printf("ERROR: can't find controller gain in settings file\n");
		return -1;
	}
	// skip check here since "1" is not considered a double during check
	// if(json_object_is_type(tmp, json_type_double)==0){
	// 	printf("ERROR: controller gain should be a double\n");
	// 	return -1;
	// }
	tmp_flt = json_object_get_double(tmp);


	// pull out numerator
	if(json_object_object_get_ex(jobj, "numerator", &array)==0){
		printf("ERROR: can't find controller numerator in settings file\n");
		return -1;
	}
	if(json_object_is_type(array, json_type_array)==0){
		printf("ERROR: controller numerator should be an array\n");
		return -1;
	}
	num_len = json_object_array_length(array);
	if(num_len<1){
		printf("ERROR, numerator must have at least 1 entry\n");
		return -1;
	}
	rc_alloc_vector(&num_vec,num_len);
	for(i=0;i<num_len;i++){
		tmp = json_object_array_get_idx(array,i);
		if(json_object_is_type(tmp, json_type_double)==0){
			printf("ERROR: numerator array entries should be a doubles\n");
			return -1;
		}
		tmp_flt = json_object_get_double(tmp);
		num_vec.d[i]=tmp_flt;
	}


	// pull out denominator
	if(json_object_object_get_ex(jobj, "denominator", &array)==0){
		printf("ERROR: can't find controller denominator in settings file\n");
		return -1;
	}
	if(json_object_is_type(array, json_type_array)==0){
		printf("ERROR: controller denominator should be an array\n");
		return -1;
	}
	den_len = json_object_array_length(array);
	if(den_len<1){
		printf("ERROR, denominator must have at least 1 entry\n");
		return -1;
	}
	rc_alloc_vector(&den_vec,den_len);
	for(i=0;i<den_len;i++){
		tmp = json_object_array_get_idx(array,i);
		if(json_object_is_type(tmp, json_type_double)==0){
			printf("ERROR: denominator array entries should be a doubles\n");
			return -1;
		}
		tmp_flt = json_object_get_double(tmp);
		den_vec.d[i]=tmp_flt;
	}

	// check for improper TF
	if(num_len>den_len){
		printf("ERROR: improper transfer function\n");
		rc_free_vector(&num_vec);
		rc_free_vector(&den_vec);
		return -1;
	}

	// check CT continuous time or DT discrete time
	if(json_object_object_get_ex(jobj, "CT_or_DT", &tmp)==0){
		printf("ERROR: can't find CT_or_DT in settings file\n");
		return -1;
	}
	if(json_object_is_type(tmp, json_type_string)==0){
		printf("ERROR: CT_or_DT should be a string\n");
		return -1;
	}
	tmp_str = (char*)json_object_get_string(tmp);


	// if CT, use tustin's approx to get to DT
	if(strcmp(tmp_str, "CT") == 0){
		// get the crossover frequency
		if(json_object_object_get_ex(jobj, "crossover_freq_rad_per_sec", &tmp)==0){
			printf("ERROR: can't find crossover frequency in settings file\n");
			return -1;
		}
		if(json_object_is_type(tmp, json_type_double)==0){
			printf("ERROR: crossover frequency should be a double\n");
			return -1;
		}
		tmp_flt = json_object_get_double(tmp);
		if(rc_c2d_tustin(filter,num_vec, den_vec, dt, tmp_flt)){
			printf("ERROR: failed to c2dtustin while parsing json\n");
			return -1;
		}
	}

	// if DT, much easier, just construct filter
	else if(strcmp(tmp_str, "DT") == 0){
		if(rc_alloc_filter(filter,num_vec, den_vec, dt)){
			printf("ERROR: failed to alloc filter in parse_controller()");
			return -1;
		}
	}

	// wrong value for CT_or_DT
	else{
		printf("ERROR: CT_or_DT must be 'CT' or 'DT'\n");
		printf("instead got :%s\n", tmp_str);
		return -1;
	}


	return 0;
}


/*******************************************************************************
* int get_json_roll_controller(rc_filter_t* ctrl)
*
* gets the controllers read from the last json read.
* returns 0 on success or -1 on failure
*******************************************************************************/
int get_json_roll_controller(rc_filter_t* ctrl){
	if(was_load_successful == 0){
		printf("ERROR: can't get json controller, last read failed\n");
		return -1;
	}
	*ctrl = roll_controller;
	return 0;
}

/*******************************************************************************
* int get_json_pitch_controller(rc_filter_t* ctrl)
*
* gets the controllers read from the last json read.
* returns 0 on success or -1 on failure
*******************************************************************************/
int get_json_pitch_controller(rc_filter_t* ctrl){
	if(was_load_successful == 0){
		printf("ERROR: can't get json controller, last read failed\n");
		return -1;
	}
	*ctrl = pitch_controller;
	return 0;
}

/*******************************************************************************
* int get_json_yaw_controller(rc_filter_t* ctrl)
*
* gets the controllers read from the last json read.
* returns 0 on success or -1 on failure
*******************************************************************************/
int get_json_yaw_controller(rc_filter_t* ctrl){
	if(was_load_successful == 0){
		printf("ERROR: can't get json controller, last read failed\n");
		return -1;
	}
	*ctrl = yaw_controller;
	return 0;
}

/*******************************************************************************
* int get_json_altitude_controller(rc_filter_t* ctrl)
*
* gets the controllers read from the last json read.
* returns 0 on success or -1 on failure
*******************************************************************************/
int get_json_altitude_controller(rc_filter_t* ctrl){
	if(was_load_successful == 0){
		printf("ERROR: can't get json controller, last read failed\n");
		return -1;
	}
	*ctrl = altitude_controller;
	return 0;
}

