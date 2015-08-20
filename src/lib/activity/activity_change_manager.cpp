#include "activity_change_manager.hpp"
#include "activity_lib_constants.h"

#include <drivers/drv_hrt.h>

#include <systemlib/param/param.h>
#include <string.h>

namespace Activity {

ActivityChangeManager::ActivityChangeManager() : 
    activity(0),
    param_count(ALLOWED_PARAM_COUNT),
    cur_param_id(0),
    params_up_to_date(false),
    activity_params_sub(-1){
}

ActivityChangeManager::ActivityChangeManager(int _activity) :
    activity(_activity),
    param_count(ALLOWED_PARAM_COUNT),
    cur_param_id(0),
    params_up_to_date(false),
    activity_params_sub(-1)
{

        if (!allowed_params_inited) init_allowed_params();
        if (!activity_limits_list_inited) init_activity_limits_list();

        init_activity_limits();
        
        activity_params_sub = orb_subscribe(ORB_ID(activity_params));

        request_dog_params();

}

ActivityChangeManager::~ActivityChangeManager() {
   orb_unsubscribe(activity_params_sub);
}

bool
ParamChangeManager::get_param_name(char * buffer, const int buffer_len){
    strncpy ( buffer, ALLOWED_PARAMS[p_id].name, buffer_len );
    return true; 
}

bool
ParamChangeManager::get_display_name(char * buffer, const int buffer_len){
    strncpy ( buffer, ALLOWED_PARAMS[p_id].display_name, buffer_len );
    return true; 
}

bool
ParamChangeManager::get_display_value(char * buffer, const int buffer_len){

    switch (limits->type){
        case PARAM_LIMIT_TYPE::VALUES_INT: 
            snprintf(buffer, buffer_len, "%i", (int)value);
            return true;
        case PARAM_LIMIT_TYPE::INTERVAL:
        case PARAM_LIMIT_TYPE::VALUES_FLOAT:
            snprintf(buffer, buffer_len, "%.2f", (double)value);
            return true;
        case PARAM_LIMIT_TYPE::VALUES_STR:

            if ( !(value >= 0 && value <= ALLOWED_PARAMS[p_id].display_value_count) ) {
               printf("Value %i out of range.\n", (int)value);
               strncpy(buffer, "No value", buffer_len);
            } else {
                strncpy(buffer, ALLOWED_PARAMS[p_id].display_values[(int)value], buffer_len);
            }
            return true;
        default:
            strncpy(buffer, "No such type", buffer_len);
            return false;
    }
}

int 
ParamChangeManager::move_value(int step_dir){

    bool found = false;
    int ind = 0;

    float eps = 1e-6;

    for (;ind < (limits->value_num); ind++ ) {
        if (float_eq(value, limits->values[ind]) && value -eps < limits->values[ind]) {
            found = true;
            break;
        }
    }

    if (found == false)
        ind = 0;
    else 
        ind = ind+step_dir;

    if (ind < 0) ind += limits->value_num;
    if (ind >= limits->value_num) ind -= limits->value_num;

    value = (limits->values[ind]);

    return 0;
}


int 
ParamChangeManager::move_interval(int step_dir){

    float istart = (limits->istart);
    float iend = (limits->iend);
    float step = (limits->step);

    if (value < istart) 
        value = istart;
    else 
    if (value > iend) 
        value = iend;
    else 
        {
            if (float_eq(value, iend) && step_dir == 1)
                value = istart;
            else
            if (float_eq(value, istart) && step_dir == -1) 
                value = iend;
            else 
                {

                    float tmp_val = istart;
                    float next_tmp_val;

                    for (;tmp_val<=iend;tmp_val+=step) {
                        
                        next_tmp_val = tmp_val + step;

                        if (step_dir == 1) {
                            if (value >= tmp_val  && value < next_tmp_val) {
                                value = next_tmp_val;
                                break;
                            }
                        }

                        if (step_dir == -1) {
                            if (value > tmp_val  && value <= next_tmp_val) {
                                value = tmp_val;
                                break;
                            }
                        }
                    }
                }
        }
    return 0;
}


bool
ParamChangeManager::get_next_value(char * buffer, int buffer_len){

    switch ( limits->type ){
        case PARAM_LIMIT_TYPE::INTERVAL:
            move_interval(1);
        break;
        case PARAM_LIMIT_TYPE::VALUES_INT: 
        case PARAM_LIMIT_TYPE::VALUES_FLOAT:
        case PARAM_LIMIT_TYPE::VALUES_STR:
            move_value(1);
        break;
    }

    return get_display_value(buffer, buffer_len);
}

bool
ParamChangeManager::get_prev_value(char * buffer, int buffer_len){

    switch ( limits->type ){
        case PARAM_LIMIT_TYPE::INTERVAL:
            move_interval(-1);
        break;
        case PARAM_LIMIT_TYPE::VALUES_INT: 
        case PARAM_LIMIT_TYPE::VALUES_FLOAT:
        case PARAM_LIMIT_TYPE::VALUES_STR:
            move_value(-1);
        break;
    }

    return get_display_value(buffer, buffer_len);
}

int
ParamChangeManager::save_value(){

    saved_value = value;
    return 0;
}

int
ParamChangeManager::cancel_value(){
    value = saved_value;
    return 0;
}

int
ParamChangeManager::get_id(){
    return p_id;
}

bool
ActivityChangeManager::get_display_name(char * buffer, const int buffer_len){
   snprintf(buffer, buffer_len, "%s", ACTIVITY_LIMITS_LIST[activity].name); 
   return true; 
}

ParamChangeManager *
ActivityChangeManager::get_next_visible_param(){

    int it = 0;
    while (true) {

        cur_param_id++;

        if (cur_param_id == ALLOWED_PARAM_COUNT)
            cur_param_id = 0;

        if (it>=ALLOWED_PARAM_COUNT)
            break;

        if (params[cur_param_id].limits->type != PARAM_LIMIT_TYPE::INVISIBLE) {
            break;
        }

        it++;
    }

    return &params[cur_param_id];
}


ParamChangeManager *
ActivityChangeManager::get_current_param(){

    int it = 0;
    while (true) {

        if (params[cur_param_id].limits->type != PARAM_LIMIT_TYPE::INVISIBLE) {
            break;
        }

        cur_param_id++;

        if (cur_param_id == ALLOWED_PARAM_COUNT)
            cur_param_id = 0;

        if (it>=ALLOWED_PARAM_COUNT)
            break;

        it++;
    }

    return &params[cur_param_id];
}

ParamChangeManager *
ActivityChangeManager::get_prev_visible_param(){

    int it = 0;
    while (true) {

        cur_param_id--;

        if (cur_param_id < 0) {
            cur_param_id = ALLOWED_PARAM_COUNT-1;
        }

        if (it >= ALLOWED_PARAM_COUNT)
            break;

        if (params[cur_param_id].limits->type != PARAM_LIMIT_TYPE::INVISIBLE) {
           break;
         }

        it++;
    }

    return &params[cur_param_id];
}

bool
ActivityChangeManager::process_received_params(activity_params_s activity_params){

    activity = activity_params.values[0];

    init_activity_limits();

    for (int i=0;i<ALLOWED_PARAM_COUNT;i++) {

        params[i].saved_value = activity_params.values[i]; 
        params[i].value = activity_params.values[i]; 

        if (ALLOWED_PARAMS[i].target_device == ALL || ALLOWED_PARAMS[i].target_device == LEASH) {

            if (param_set(param_find(ALLOWED_PARAMS[i].name), &activity_params.values[i]) != 0) {
                return false;
            }

        }
    }

    params_up_to_date = true;
    cur_param_id = 0;

    return true;
}

bool
ActivityChangeManager::init_activity_limits() {

    for (int i=0;i<ALLOWED_PARAM_COUNT;i++) {

        params[i].p_id = i;
        params[i].limits = nullptr;

        for (int j=0;j<ACTIVITY_LIMITS_LIST[activity].param_count;j++)
            if (ACTIVITY_LIMITS_LIST[0].params[i].p_id == ACTIVITY_LIMITS_LIST[activity].params[j].p_id) {
                params[i].limits = &ACTIVITY_LIMITS_LIST[activity].params[j];
            }

        if (params[i].limits == nullptr) {
            params[i].limits = &ACTIVITY_LIMITS_LIST[0].params[i];
        }
    }
}

bool
ActivityChangeManager::save_params(){

    activity_params_s activity_params;

    activity_params.type = ACTIVITY_PARAMS_SAVED;
    activity_params.ts = hrt_absolute_time();

    for (int i=0;i<ALLOWED_PARAM_COUNT;i++) {
        activity_params.values[i] = params[i].saved_value;
    }

    int activity_params_pub = orb_advertise(ORB_ID(activity_params), &activity_params);

    if (activity_params_pub <= 0) {
        printf("Failed to publish activity params to orb.\n");
        return false;
    }

    send_params_to_dog();

    return true;

}

int
ActivityChangeManager::cancel_params(){

    activity_params_s activity_params;
    orb_copy(ORB_ID(activity_params), activity_params_sub, &activity_params);

    for (int i=0;i<ALLOWED_PARAM_COUNT;i++) {

        params[i].saved_value = activity_params.values[i]; 
        params[i].value = activity_params.values[i]; 

    }

}

bool
ActivityChangeManager::send_params_to_dog(){

    activity_params_sndr_s activity_params_sndr;
    activity_params_sndr.type = ACTIVITY_PARAMS_MSG_VALUES;

    int activity_params_sndr_pub = orb_advertise(ORB_ID(activity_params_sndr), &activity_params_sndr);

    if (activity_params_sndr_pub <= 0){
        printf("Failed to publish activity params sender to orb.\n");
        return false;
    }

    params_up_to_date = false;
}

bool 
ActivityChangeManager::params_received() {

    if (params_up_to_date)
        return true;
    else  {
    
        bool updated = false;
        orb_check(activity_params_sub, &updated);

        if (updated) {

            activity_params_s activity_params;
            orb_copy(ORB_ID(activity_params), activity_params_sub, &activity_params);

            if (activity_params.type == ACTIVITY_PARAMS_RECEIVED) {
                process_received_params(activity_params);
                params_up_to_date = true;
            }
        }
    }

    return params_up_to_date;
}

bool 
ActivityChangeManager::request_dog_params() {

    activity_request_sndr_s activity_request_sndr;
    activity_request_sndr.type = ACTIVITY_REQUEST_PARAMS; 

    int activity_request_sndr_pub = orb_advertise(ORB_ID(activity_request_sndr), &activity_request_sndr);

    if (activity_request_sndr_pub <= 0) {
        printf("Error: failed to publish to activity request sender orb! ");
    }

}

bool
float_eq(float a, float b) {
    float eps = 1e-6;
    return (a+eps > b && a-eps < b);
}

}
// End of Activity namespace
