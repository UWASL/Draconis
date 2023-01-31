/* -*- P4_14 -*- */

#ifdef __TARGET_TOFINO__
#include <tofino/constants.p4>
#include <tofino/intrinsic_metadata.p4>
#include <tofino/primitives.p4>
#include <tofino/stateful_alu_blackbox.p4>
#else
#error This program is intended to compile for Tofino P4 architecture only
#endif

#include "register_instantiate.p4"

#define QUEUE_SIZE 524288
// #define QUEUE_SIZE                      131072
// #define QUEUE_SIZE_ALL_ONES             65535

#define QUEUE_SIZE_ALL_ONES             QUEUE_SIZE-1
#define MSB_32_BIT_SET                  2147483648

#define LOOPBACK_PORT                   68
#define P4_SWITCH_IP                    0xC0A87E37

#define RETRIEVE_TASK_OPERATION_CODE    1
#define SUBMIT_JOB_OPERATION_CODE       2
#define FIX_C_OPERATION_CODE            3
#define FIX_P_OPERATION_CODE            4
#define FINISHED_TASK_OPERATION_CODE    5
#define TASKS_QUEUE_IS_FULL             -1



/*************************************************************************
 ***********************  H E A D E R S  *********************************
 *************************************************************************/
// Basic Ethernet Header
header_type ethernet_t {
    fields {
        dstAddr   : 48;
        srcAddr   : 48;
        etherType : 16;
    }
}

// IPV4 header
header_type ipv4_t {
    fields {
        version        : 4;
        ihl            : 4;
        diffserv       : 8;
        totalLen       : 16;
        identification : 16;
        flags          : 3;
        fragOffset     : 13;
        ttl            : 8;
        protocol       : 8;
        hdrChecksum    : 16;
        srcAddr        : 32;
        dstAddr        : 32;
    }
}

// UDP Header
header_type udp_t {
    fields {
        srcPort        : 16;
        dstPort        : 16;
        len            : 16;
        checksum       : 16;
    }
}


header_type p4_scheduler_t {
    fields {
        operation_code  : 8;
    }
}

// Fields empty when sent by worker and filled out by switch
header_type p4_scheduler_retrieve_task_t {
    fields {
        user_id                             : 8;
        job_id                              : 32;
        task_id                             : 16;
        function_id                         : 16;
        function_parameters                 : 32;
        user_ip_address                     : 32;
        user_port_number                    : 16;
        beggining_scheduling_time_1         : 32;
        scheduler_recieved_get_task_time_1  : 32;
    }
}

// Header type containing tasks from user for job submission
header_type p4_scheduler_submit_job_t {
    fields {
        user_id             : 8;
        job_id              : 32;
        number_of_tasks     : 16;
        fix_array_index     : 32; // What is this field? Is it for fixing consume / produce pointers?
    }
}

// Size for the p4_scheduler_task_t header defined below converted into bytes
#define TASK_IN_BYTES 8     // this header could be removed from the packet; hence, we need
                            // to subtract its size from ipv4 & udp headers

// Header to represent one task in a job with parameters
header_type p4_scheduler_task_t {
    fields {
        function_id         : 16;
        function_parameters  : 32;
        task_id             : 16;      
    }
}

// Header to manage finished tasks
header_type p4_scheduler_finished_task_t {
    fields {
        user_id                  : 8;
        job_id                   : 32;
        task_id                  : 16;
        task_status              : 8;

        client_ip                : 32;
        client_port              : 16;

        // Currently passed through the switch for metrics
        scheduler_beggining_time : 32; // Figure out what these 2 are for - Metrics?
        scheduler_end_time       : 32;
        time1: 32;
        time2: 32;
        time3: 32;
    }
}

header_type my_metadata_t {
    fields {
        udp_len : 16;
        ipv4_totalLen : 16;

        number_of_tasks : 8;

        shifted_ingress_global_tstamp : 16;


        recieved_packet_src_ip   :32;   // these metadata fields are used to reflect the packet to the sender
        recieved_packet_dst_ip   :32;
        recieved_packet_src_port :16;
        recieved_packet_dst_port :16;

        p_minus_c                :32;   // P - C (used for correctness checks)
        queue_size_minus_p_minus_c :32;   // (P - C) - QueueSize (used for correctness checks)

        should_fix_p             :1;
        should_fix_c             :1;

        read_destination_global_sequence_number : 32;

        read_destination_produce_index : 32;
        write_data_produce_index : 32;
        operation_produce_index : 2;

        read_destination_consume_index : 32;
        write_data_consume_index : 32;
        operation_consume_index : 2;

        index_functions_ids :32;
        read_destination_functions_ids :16;
        write_data_functions_ids :16;
        will_write_field_functions_ids :1;

        index_functions_parameters :32;
        read_destination_functions_parameters :32;
        write_data_functions_parameters :32;
        will_write_field_functions_parameters :1;

        index_tasks_ids :32;
        read_destination_tasks_ids :16;
        write_data_tasks_ids :16;
        will_write_field_tasks_ids :1;

        index_users_ips :32;
        read_destination_users_ips :32;
        write_data_users_ips :32;
        will_write_field_users_ips :1;

        index_users_ports :32;
        read_destination_users_ports :16;
        write_data_users_ports :16;
        will_write_field_users_ports :1;

        read_destination_fix_c: 1;
        write_data_fix_c: 1;
        will_write_field_fix_c: 1;

        read_destination_fix_p: 1;
        write_data_fix_p: 1;
        will_write_field_fix_p: 1;

        index_users_ids :32;
        read_destination_users_ids :8;
        write_data_users_ids :8;
        will_write_field_users_ids :1;

        index_jobs_ids :32;
        read_destination_jobs_ids :32;
        write_data_jobs_ids : 32;
        will_write_field_jobs_ids :1;

        // 2 beginning scheduling times because timestamp is 48 bits but probably doesnt work correctly yet
        index_beggining_scheduling_time_1 :32;
        read_destination_beggining_scheduling_time_1 :32;
        write_data_beggining_scheduling_time_1 :32;
        will_write_field_beggining_scheduling_time_1 :1;

        /*index_beggining_scheduling_time_2 :32;
        read_destination_beggining_scheduling_time_2 :16;
        write_data_beggining_scheduling_time_2 :16;
        will_write_field_beggining_scheduling_time_2 :1;*/
    }
}



/*************************************************************************
 ***********************  M E T A D A T A  *******************************
 *************************************************************************/
metadata my_metadata_t my_metadata;
metadata p4_scheduler_task_t current_task;


/*************************************************************************
 ***********************  P A R S E R  ***********************************
 *************************************************************************/
header ethernet_t  ethernet;
header ipv4_t      ipv4;
header udp_t       udp;
header p4_scheduler_t   p4_scheduler;
header p4_scheduler_retrieve_task_t   p4_scheduler_retrieve_task;
header p4_scheduler_submit_job_t   p4_scheduler_submit_job;
header p4_scheduler_task_t   p4_scheduler_task;
header p4_scheduler_finished_task_t   p4_scheduler_finished_task;

// Parser starts here
parser start {
    extract(ethernet);
    // What are returns here? Isnt it supposed to be transition? Old syntax?
    return select(ethernet.etherType) {
        // If etherType == IPv4, go to IPv4 otherwise just push it through
        0x0800 : parse_ipv4;
        default: ingress;
    }
}

parser parse_ipv4 {
    extract(ipv4);
    return select(ipv4.protocol) {
        // Accept others like TCP while moving to parse_udp for UDP
        17 : parse_udp;
        default: ingress;
    }
}

parser parse_udp {
    extract(udp);
    // Does this select merge both fields and then compare to the masked value?
    return select(udp.srcPort, udp.dstPort){      // if either src or dst ports == 5555, then it's our protocol
        0x15b30000 mask 0xffff0000 : parse_p4_scheduler;
        0x000015b3 mask 0x0000ffff : parse_p4_scheduler;
        default : ingress;
    }
}

// Parser Main State for packets belonging to our protocol
// Defaults to submit_job
parser parse_p4_scheduler {
    extract(p4_scheduler);
    return select(p4_scheduler.operation_code) {
        RETRIEVE_TASK_OPERATION_CODE : parse_p4_scheduler_retrieve_task;
        FINISHED_TASK_OPERATION_CODE : parse_p4_scheduler_finished_task;
        default: parse_p4_scheduler_submit_job;
    }
}

// Probably for worker asking for task?
parser parse_p4_scheduler_retrieve_task {
    extract(p4_scheduler_retrieve_task);
    return ingress;
}

// Packet to notify scheduler of finished task
parser parse_p4_scheduler_finished_task {
    extract(p4_scheduler_finished_task);
    return ingress;
}

// Header for job submission
parser parse_p4_scheduler_submit_job {
    extract(p4_scheduler_submit_job);
    return parse_p4_scheduler_tasks;
}

// Support state for job submission
// Supports extract one task and recirculate I think
parser parse_p4_scheduler_tasks {
    extract(p4_scheduler_task);
    return ingress;
}

// Field_list and calculated_field taken from skeleton P4 code for IPv4 checksum
field_list ipv4_checksum_fields {
    ipv4.version;
    ipv4.ihl;
    ipv4.diffserv;
    ipv4.totalLen;
    ipv4.identification;
    ipv4.flags;
    ipv4.fragOffset;
    ipv4.ttl;
    ipv4.protocol;
    ipv4.srcAddr;
    ipv4.dstAddr;
}

field_list_calculation ipv4_checksum {
    input         { ipv4_checksum_fields; }
    algorithm:    csum16;
    output_width: 16;
}

calculated_field ipv4.hdrChecksum {
    verify ipv4_checksum;
    update ipv4_checksum;
}

field_list random_fields {
    ipv4.totalLen;
    ipv4.ttl;
    ipv4.srcAddr;
    ig_intr_md_from_parser_aux.ingress_global_tstamp;
}

field_list_calculation random_range {
    input         { random_fields; }
    algorithm:    random;
    output_width: 64;
}






/*************************************************************************
 **************  R E G I S T E R   C R E A T I O N   *********************
 *************************************************************************/

// Quick summary of these support functions? Are they to initialize the state of all registers?

// TODO Delete and test with switch
create_increment_counter_register(register_region_config_global_sequence_number, 32, 1, 0, my_metadata.read_destination_global_sequence_number)

create_read_and_write_and_inc_register(register_produce_index, 32, 1, 0, my_metadata.read_destination_produce_index, my_metadata.write_data_produce_index, my_metadata.operation_produce_index)
create_read_and_write_and_inc_register(register_consume_index, 32, 1, 0, my_metadata.read_destination_consume_index, my_metadata.write_data_consume_index, my_metadata.operation_consume_index)

create_read_and_write_register(register_functions_ids, 16, QUEUE_SIZE, my_metadata.index_functions_ids, my_metadata.read_destination_functions_ids, my_metadata.write_data_functions_ids, my_metadata.will_write_field_functions_ids)
create_read_and_write_register(register_functions_parameters, 32, QUEUE_SIZE, my_metadata.index_functions_parameters, my_metadata.read_destination_functions_parameters, my_metadata.write_data_functions_parameters, my_metadata.will_write_field_functions_parameters)
create_read_and_write_register(register_tasks_ids, 16, QUEUE_SIZE, my_metadata.index_tasks_ids, my_metadata.read_destination_tasks_ids, my_metadata.write_data_tasks_ids, my_metadata.will_write_field_tasks_ids)
create_read_and_write_register(register_users_ips, 32, QUEUE_SIZE, my_metadata.index_users_ips, my_metadata.read_destination_users_ips, my_metadata.write_data_users_ips, my_metadata.will_write_field_users_ips)
create_read_and_write_register(register_users_ports, 16, QUEUE_SIZE, my_metadata.index_users_ports, my_metadata.read_destination_users_ports, my_metadata.write_data_users_ports, my_metadata.will_write_field_users_ports)
create_read_and_write_register(register_users_ids, 8, QUEUE_SIZE, my_metadata.index_users_ids, my_metadata.read_destination_users_ids, my_metadata.write_data_users_ids, my_metadata.will_write_field_users_ids)
create_read_and_write_register(register_jobs_ids, 32, QUEUE_SIZE, my_metadata.index_jobs_ids, my_metadata.read_destination_jobs_ids, my_metadata.write_data_jobs_ids, my_metadata.will_write_field_jobs_ids)
create_read_and_write_register(register_beggining_scheduling_time_1, 32, QUEUE_SIZE, my_metadata.index_beggining_scheduling_time_1, my_metadata.read_destination_beggining_scheduling_time_1, my_metadata.write_data_beggining_scheduling_time_1, my_metadata.will_write_field_beggining_scheduling_time_1)
// create_read_and_write_register(register_beggining_scheduling_time_2, 16, QUEUE_SIZE, my_metadata.index_beggining_scheduling_time_2, my_metadata.read_destination_beggining_scheduling_time_2, my_metadata.write_data_beggining_scheduling_time_2, my_metadata.will_write_field_beggining_scheduling_time_2)


// Flags to indicate that consumer or producers are being fixed - Currently 8 bits because 1 bit syntax isnt clear
create_read_and_write_register(register_fix_c, 8, 1, 0, my_metadata.read_destination_fix_c, my_metadata.write_data_fix_c, my_metadata.will_write_field_fix_c)
create_read_and_write_register(register_fix_p, 8, 1, 0, my_metadata.read_destination_fix_p, my_metadata.write_data_fix_p, my_metadata.will_write_field_fix_p)

/*************************************************************************
 **************  I N G R E S S   P R O C E S S I N G   *******************
 *************************************************************************/

// Drop and quit processing
action discard() {
    drop();
    exit();
}

// Just drop
action dont_forward() {
    drop();
}

// Action to remove one task from a submit job header
action remove_task() {
    remove_header(p4_scheduler_task);
    add(ipv4.totalLen, -TASK_IN_BYTES, my_metadata.ipv4_totalLen);
    add(udp.len, -TASK_IN_BYTES,my_metadata.udp_len);
    add(p4_scheduler_submit_job.number_of_tasks, -1, my_metadata.number_of_tasks);
}

// Basic L3 forwarding
action action_layer_3_switching(port) {     
    modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
}

// Recirculation for job submit packets to populate tasks
action do_recirculate(){
    recirculate(LOOPBACK_PORT);
}

// Called when worker wants a task?
action action_retrieve_task(){
    modify_field(my_metadata.operation_produce_index, REGISTER_OPERATION_READ);
    modify_field(my_metadata.operation_consume_index, REGISTER_OPERATION_INC);

    modify_field(my_metadata.will_write_field_functions_ids, 1);
    modify_field(my_metadata.will_write_field_functions_parameters, 1);
    modify_field(my_metadata.will_write_field_tasks_ids, 1);
    modify_field(my_metadata.will_write_field_users_ips, 1);
    modify_field(my_metadata.will_write_field_users_ports, 1);

    modify_field(my_metadata.will_write_field_users_ids, 1);
    modify_field(my_metadata.will_write_field_jobs_ids, 1);
    modify_field(my_metadata.will_write_field_beggining_scheduling_time_1, 1);
    // modify_field(my_metadata.will_write_field_beggining_scheduling_time_2, 1);

    // Update last getTask() receival time
    modify_field(p4_scheduler_retrieve_task.scheduler_recieved_get_task_time_1, ig_intr_md_from_parser_aux.ingress_global_tstamp);

}

// Called to handle job submissions
action action_submit_job(){
    // For removing packet headers (tasks from a submit job) - Adding with 0 instead of move because possible errors due to too many arith ops
    add(my_metadata.ipv4_totalLen,  ipv4.totalLen,0);    // useful if a header is removed from the packet
    add(my_metadata.udp_len, udp.len,0);
    add(my_metadata.number_of_tasks, p4_scheduler_submit_job.number_of_tasks,0);

    modify_field(my_metadata.operation_produce_index, REGISTER_OPERATION_INC);
    modify_field(my_metadata.operation_consume_index, REGISTER_OPERATION_READ);

    // Assume it is to be fixed here - maybe cleared later
    modify_field(my_metadata.will_write_field_fix_p, 1);
    modify_field(my_metadata.will_write_field_fix_c, 1);

    modify_field(my_metadata.will_write_field_functions_ids, 1);
    modify_field(my_metadata.will_write_field_functions_parameters, 1);
    modify_field(my_metadata.will_write_field_tasks_ids, 1);
    modify_field(my_metadata.will_write_field_users_ips, 1);
    modify_field(my_metadata.will_write_field_users_ports, 1);
    modify_field(my_metadata.will_write_field_users_ids, 1);
    modify_field(my_metadata.will_write_field_jobs_ids, 1);
    modify_field(my_metadata.will_write_field_beggining_scheduling_time_1, 1);
    // modify_field(my_metadata.will_write_field_beggining_scheduling_time_2, 1);
}

action action_fix_c(){
    modify_field(my_metadata.operation_produce_index, REGISTER_OPERATION_READ);
    modify_field(my_metadata.operation_consume_index, REGISTER_OPERATION_WRITE);
    modify_field(my_metadata.will_write_field_fix_c, 1);
}

action action_fix_p(){
    modify_field(my_metadata.operation_produce_index, REGISTER_OPERATION_WRITE);
    modify_field(my_metadata.operation_consume_index, REGISTER_OPERATION_READ);
    modify_field(my_metadata.will_write_field_fix_p, 1);
}

// Action to handle finished tasks
action action_finished_task(){
    // Last task finish time - Metrics only ?
    modify_field(p4_scheduler_finished_task.scheduler_end_time, ig_intr_md_from_parser_aux.ingress_global_tstamp);

}

// Write placeholders for empty into registers here?
action action_retrieve_task_write_data(){
    modify_field(my_metadata.write_data_functions_ids, -1);
    modify_field(my_metadata.write_data_functions_parameters, -1);
    modify_field(my_metadata.write_data_tasks_ids, -1);
    modify_field(my_metadata.write_data_users_ips, -1);
    modify_field(my_metadata.write_data_users_ports, -1);
    modify_field(my_metadata.write_data_users_ids, -1);
    modify_field(my_metadata.write_data_jobs_ids, -1);
    modify_field(my_metadata.write_data_beggining_scheduling_time_1, -1);
    // modify_field(my_metadata.write_data_beggining_scheduling_time_2, -1);
}

// Writes values for task submission into registers
action action_submit_job_write_data(){
    modify_field(my_metadata.write_data_functions_ids, p4_scheduler_task.function_id);
    modify_field(my_metadata.write_data_functions_parameters, p4_scheduler_task.function_parameters); 
    add(my_metadata.write_data_tasks_ids, p4_scheduler_task.task_id,0);
    add(my_metadata.write_data_users_ips, ipv4.srcAddr,0);
    add(my_metadata.write_data_users_ports, udp.srcPort,0);

    add(my_metadata.write_data_users_ids, p4_scheduler_submit_job.user_id,0);
    add(my_metadata.write_data_jobs_ids, p4_scheduler_submit_job.job_id,0);

    modify_field(my_metadata.write_data_beggining_scheduling_time_1, ig_intr_md_from_parser_aux.ingress_global_tstamp);
    // modify_field(my_metadata.write_data_beggining_scheduling_time_2,my_metadata.shifted_ingress_global_tstamp);

}

// Fixing consume pointer
action action_fix_c_write_data(){
    modify_field(my_metadata.write_data_consume_index, p4_scheduler_submit_job.fix_array_index);
    modify_field(my_metadata.write_data_fix_c, 0);
}

// Fixing produce pointer
action action_fix_p_write_data(){
    modify_field(my_metadata.write_data_produce_index, p4_scheduler_submit_job.fix_array_index);
    modify_field(my_metadata.write_data_fix_p, 0);
}

// Writing in vaues for finished tasks
action action_finished_task_write_data(){
    modify_field(udp.dstPort, p4_scheduler_finished_task.client_port);
    modify_field(udp.srcPort, 5555);
    modify_field(ipv4.srcAddr, P4_SWITCH_IP);
    modify_field(ipv4.dstAddr, p4_scheduler_finished_task.client_ip);
}

// What do these 2 functions do?
action action_fill_index_array_consume(){
    // First read consume index, then move it to index_function_ids to allow the function IDs to be indexed into via the consume index
    // BIT_AND is used here due to weird compiler behavior with arith ops
    bit_and(my_metadata.index_functions_ids, my_metadata.read_destination_consume_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_functions_parameters, my_metadata.read_destination_consume_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_tasks_ids, my_metadata.read_destination_consume_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_users_ips, my_metadata.read_destination_consume_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_users_ports, my_metadata.read_destination_consume_index, QUEUE_SIZE_ALL_ONES);

    bit_and(my_metadata.index_users_ids, my_metadata.read_destination_consume_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_jobs_ids, my_metadata.read_destination_consume_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_beggining_scheduling_time_1, my_metadata.read_destination_consume_index, QUEUE_SIZE_ALL_ONES);
    // bit_and(my_metadata.index_beggining_scheduling_time_2, my_metadata.read_destination_consume_index, QUEUE_SIZE_ALL_ONES);
}

// Called for everything except consume, not just produce
action action_fill_index_array_not_consume(){
    bit_and(my_metadata.index_functions_ids, my_metadata.read_destination_produce_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_functions_parameters, my_metadata.read_destination_produce_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_tasks_ids, my_metadata.read_destination_produce_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_users_ips, my_metadata.read_destination_produce_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_users_ports, my_metadata.read_destination_produce_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_users_ids, my_metadata.read_destination_produce_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_jobs_ids, my_metadata.read_destination_produce_index, QUEUE_SIZE_ALL_ONES);
    bit_and(my_metadata.index_beggining_scheduling_time_1, my_metadata.read_destination_produce_index, QUEUE_SIZE_ALL_ONES);
    // bit_and(my_metadata.index_beggining_scheduling_time_2, my_metadata.read_destination_produce_index, QUEUE_SIZE_ALL_ONES);

    // P - C is the curr number of tasks sitting in queue (Used to check if queue is full, if consumer is ahead of producer or normal ops)
    subtract(my_metadata.p_minus_c, my_metadata.read_destination_produce_index, my_metadata.read_destination_consume_index); //calculate p-c
}

// Use metadata queue suze (curr size) and subtract it from max possible queue size and store in queue_size_p_minus_c
action action_queue_size_minus_p_minus_c(){
    subtract(my_metadata.queue_size_minus_p_minus_c, QUEUE_SIZE - 1, my_metadata.p_minus_c);
}

// Check if its negative using MSB - USed for later computations
// Either of these set = 2 diff scenarios -> p_minus_c negative => C Needs fixing because C > P
// Queue_size negative = Queue full => P incremented wrongly => Fix P
// These are esclusive scenarios i.e. both can't occur
action action_compute_p_minus_c_and_queue_size_bitwise_and(){
    bit_and(my_metadata.p_minus_c, my_metadata.p_minus_c, MSB_32_BIT_SET);
    bit_and(my_metadata.queue_size_minus_p_minus_c, my_metadata.queue_size_minus_p_minus_c, MSB_32_BIT_SET);
}

action action_populate_retrieve_task_packet(){
    modify_field(p4_scheduler_retrieve_task.function_id , my_metadata.read_destination_functions_ids);
    modify_field(p4_scheduler_retrieve_task.function_parameters , my_metadata.read_destination_functions_parameters);
    modify_field(p4_scheduler_retrieve_task.task_id , my_metadata.read_destination_tasks_ids);
    modify_field(p4_scheduler_retrieve_task.user_ip_address , my_metadata.read_destination_users_ips);
    modify_field(p4_scheduler_retrieve_task.user_port_number , my_metadata.read_destination_users_ports);

    modify_field(p4_scheduler_retrieve_task.user_id , my_metadata.read_destination_users_ids);
    modify_field(p4_scheduler_retrieve_task.job_id , my_metadata.read_destination_jobs_ids);
    modify_field(p4_scheduler_retrieve_task.beggining_scheduling_time_1 , my_metadata.read_destination_beggining_scheduling_time_1);

}

// Reflect back to worker? - Set srcPort to 5555 since worker exepcts response from that port
action action_reflect_retrieve_task_packet(){
    modify_field(udp.srcPort, 5555);
    modify_field(udp.dstPort, my_metadata.recieved_packet_src_port);
    modify_field(ipv4.srcAddr, my_metadata.recieved_packet_dst_ip);
    modify_field(ipv4.dstAddr, my_metadata.recieved_packet_src_ip);
}

action action_fill_layer3_and_layer4_metadata(){
    modify_field(my_metadata.recieved_packet_src_ip , ipv4.srcAddr);
    modify_field(my_metadata.recieved_packet_dst_ip , ipv4.dstAddr);
    modify_field(my_metadata.recieved_packet_src_port , udp.srcPort);
    modify_field(my_metadata.recieved_packet_dst_port , udp.dstPort);
    modify_field_with_shift(my_metadata.shifted_ingress_global_tstamp, ig_intr_md_from_parser_aux.ingress_global_tstamp, 48, 0xffff);
}

action action_set_should_fix_c(){
    modify_field(my_metadata.should_fix_c , 1);
    modify_field(my_metadata.write_data_fix_c , 1);
}

action action_set_should_fix_p(){
    modify_field(my_metadata.should_fix_p , 1);
    modify_field(my_metadata.write_data_fix_p , 1);

    modify_field(my_metadata.will_write_field_functions_ids, 0);
    modify_field(my_metadata.will_write_field_functions_parameters, 0);
    modify_field(my_metadata.will_write_field_tasks_ids, 0);
    modify_field(my_metadata.will_write_field_users_ips, 0);
    modify_field(my_metadata.will_write_field_users_ports, 0);
    modify_field(my_metadata.will_write_field_users_ids, 0);
    modify_field(my_metadata.will_write_field_jobs_ids, 0);
    modify_field(my_metadata.will_write_field_beggining_scheduling_time_1, 0);
    // modify_field(my_metadata.will_write_field_beggining_scheduling_time_2, 0);
}

action action_done_fixing_c(){
    modify_field(p4_scheduler.operation_code, SUBMIT_JOB_OPERATION_CODE);
    modify_field(my_metadata.should_fix_p , 0); // to make sure that not another recirculate is done
    modify_field(my_metadata.should_fix_c , 0);
}

action action_done_fixing_p(){
    modify_field(p4_scheduler.operation_code, TASKS_QUEUE_IS_FULL);
    // tell the client that the queue is full, to resubmit the job (or remaining tasks)
    modify_field(udp.srcPort, 5555);
    modify_field(udp.dstPort, my_metadata.recieved_packet_src_port);
    modify_field(ipv4.srcAddr, my_metadata.recieved_packet_dst_ip);
    modify_field(ipv4.dstAddr, my_metadata.recieved_packet_src_ip);
}

action action_recirculate_to_fix_c(){
    modify_field(p4_scheduler_submit_job.fix_array_index, my_metadata.read_destination_produce_index);
    modify_field(p4_scheduler.operation_code, FIX_C_OPERATION_CODE);
    recirculate(LOOPBACK_PORT);
}

action action_recirculate_to_fix_p(){
    modify_field(p4_scheduler_submit_job.fix_array_index, my_metadata.read_destination_produce_index);
    modify_field(p4_scheduler.operation_code, FIX_P_OPERATION_CODE);
    recirculate(LOOPBACK_PORT);
}

table table_remove_task {
                                  
    actions {                                                                   
        remove_task;                                  
    }                                                                           
    default_action : remove_task;                 
    size : 1;                                                            
}
table table_reflect_retrieve_task_packet {
                                  
    actions {                                                                   
        action_reflect_retrieve_task_packet;                                  
    }                                                                           
    default_action : action_reflect_retrieve_task_packet;                 
    size : 1;                                                            
}

table table_done_fixing_c {
                                  
    actions {                                                                   
        action_done_fixing_c;                                  
    }                                                                           
    default_action : action_done_fixing_c;                 
    size : 1;                                                            
}

table table_done_fixing_p {
                                  
    actions {                                                                   
        action_done_fixing_p;                                  
    }                                                                           
    default_action : action_done_fixing_p;                 
    size : 1;                                                            
}

table table_compute_p_minus_c_and_queue_size_bitwise_and {
                                  
    actions {                                                                   
        action_compute_p_minus_c_and_queue_size_bitwise_and;                                  
    }                                                                           
    default_action : action_compute_p_minus_c_and_queue_size_bitwise_and;                 
    size : 1;                                                                   
}

table table_queue_size_minus_p_minus_c {
                                  
    actions {                                                                   
        action_queue_size_minus_p_minus_c;                                  
    }                                                                           
    default_action : action_queue_size_minus_p_minus_c;                 
    size : 1;                                                                   
}

table table_set_should_fix_c {
                                  
    actions {                                                                   
        action_set_should_fix_c;                                  
    }                                                                           
    default_action : action_set_should_fix_c;                 
    size : 1;                                                                   
}

table table_set_should_fix_p {
                                  
    actions {                                                                   
        action_set_should_fix_p;                                  
    }                                                                           
    default_action : action_set_should_fix_p;                 
    size : 1;                                                                   
}

table table_do_recirculate {
                                  
    actions {                                                                   
        do_recirculate;                                  
    }                                                                           
    default_action : do_recirculate;                 
    size : 1;                                                                   
}

table table_fill_layer3_and_layer4_metadata{                          
    actions {                                                                   
        action_fill_layer3_and_layer4_metadata;                                  
    }                                                                           
    default_action : action_fill_layer3_and_layer4_metadata;                 
    size : 1;                                                                   
}

table table_dont_forward{                          
    actions {                                                                   
        dont_forward;                                  
    }                                                                           
    default_action : dont_forward;                 
    size : 1;                                                                   
}

table table_layer_3_switching {
    reads{
        ipv4.dstAddr : exact;
    }              
    actions {                                                                   
        action_layer_3_switching;                                  
    }                                                                           
    size : 256;                                                                   
}

table table_fill_index_array {
    reads{
        p4_scheduler.operation_code : exact;
    }              
    actions {                                                                   
        action_fill_index_array_consume;                                  
        action_fill_index_array_not_consume;                                                                 
    } 
    default_action : action_fill_index_array_not_consume;                                                                                           
    size : 8;                                                                   
}

// table table_populate_retrieve_task_packet {
//     reads{
//         my_metadata.read_destination_functions_ids : exact;
//     }              
//     actions {                                                                   
//         action_populate_retrieve_task_packet;                                  
//         discard;                                                                  
//     } 
//     default_action : action_populate_retrieve_task_packet;                                                                                           
//     size : 1;                                                                   
// }

table table_populate_retrieve_task_packet {         
    actions {                                                                   
        action_populate_retrieve_task_packet;                                  
    } 
    default_action : action_populate_retrieve_task_packet;                                                                                           
    size : 1;                                                                   
}

table table_set_registers_behavior {
    reads{
        p4_scheduler.operation_code : exact;
    }              
    actions {                                                                   
        action_retrieve_task;                                  
        action_submit_job;                                  
        action_fix_c;                                  
        action_fix_p;
        action_finished_task;                                  
    } 
    default_action : discard;                                                                                           
    size : 8;                                                                   
}

table table_set_registers_write_data {
    reads{
        p4_scheduler.operation_code : exact;
    }              
    actions {                                                                   
        action_retrieve_task_write_data;                                  
        action_submit_job_write_data;                                  
        action_fix_c_write_data;                                  
        action_fix_p_write_data;
        action_finished_task_write_data;                        
    } 
    default_action : discard;                                                                                           
    size : 8;                                                                   
}

table table_decide_should_fix_c_and_p {
    reads{
        p4_scheduler.operation_code : exact;
        my_metadata.p_minus_c : exact;
        my_metadata.queue_size_minus_p_minus_c : exact;
    }              
    actions {                                                                   
        action_set_should_fix_c;
        action_set_should_fix_p;                               
    } 
    size : 2;                                                                   
}

table table_check_if_fix_recirculate_needed {
    reads{
        p4_scheduler.operation_code : exact;
        my_metadata.should_fix_c : exact;
        my_metadata.should_fix_p : exact;
        my_metadata.read_destination_fix_c : exact;
        my_metadata.read_destination_fix_p : exact;
    }              
    actions {                                                                   
        action_recirculate_to_fix_c;
        action_recirculate_to_fix_p;                               
    } 
    size : 4;                                                                   
}

table table_submit_next_task {
    reads{
        p4_scheduler_submit_job.number_of_tasks : exact;
    }              
    actions {                                                                   
        discard;
    }
    size : 1;                                                                   
}

control ingress {

    if(valid(p4_scheduler) ){

        // Routing IPs and stuff - Packet Src, Dest IP and Port
        apply(table_fill_layer3_and_layer4_metadata);

        apply(table_set_registers_behavior);   
        apply(table_set_registers_write_data);   

        // These 2 are macros too - See below
        apply(table_read_and_write_and_inc_register_register_produce_index);
        apply(table_read_and_write_and_inc_register_register_consume_index);

        apply(table_fill_index_array);      // fills the index of the tasks array, and computes (p-c)
        apply(table_queue_size_minus_p_minus_c); // computes (p-c) - queueSize
        apply(table_compute_p_minus_c_and_queue_size_bitwise_and);


        apply(table_decide_should_fix_c_and_p); // if c > p, then we should fix c
                                                // if p - c >= queue size, then should fix p

        // Simple read / write reg -> Can have either op done on it => Created using macros in register instantiate.txt
        // Useful to run single actions and save us from creating tons of tables for those
        apply(table_read_and_write_register_register_fix_c);
        apply(table_read_and_write_register_register_fix_p);


        // Last steps
        if (p4_scheduler.operation_code == SUBMIT_JOB_OPERATION_CODE){
            apply(table_remove_task);
        }
        else if (p4_scheduler.operation_code == RETRIEVE_TASK_OPERATION_CODE){
            
            // Reflect an empty packet back to worker to make it ask for another task again
            // Min Java sleep time is 1 ms so this avoids that by making the worker ask for tasks again every RTT until it gets one
            // Makes CDFs look better
            apply(table_reflect_retrieve_task_packet);

            // if retrieve task == 0 > drop the packet, else populate packet, and send to sender
            // apply(table_populate_retrieve_task_packet);
        }
        // Fixing only needs to happen in submit jobs
        else if (p4_scheduler.operation_code == FIX_C_OPERATION_CODE){
            // change it to submit job, and clear recirculate packet to fix (because fix is done already)
            apply(table_done_fixing_c);

        }
        else if (p4_scheduler.operation_code == FIX_P_OPERATION_CODE){
            // send to the client that queue is full
            apply(table_done_fixing_p);
        }


        // if fix recirculate is needed, change operation, and feed pointer and recirculate
        apply(table_check_if_fix_recirculate_needed);

        // if packet is submit job, recirculate (might be dropped in the egress if # of remaining tasks = 0)
       if (p4_scheduler.operation_code == SUBMIT_JOB_OPERATION_CODE){
            apply(table_do_recirculate);
        }

    }

    if (valid(ipv4)){
        apply(table_layer_3_switching);    
    }


}

/*************************************************************************
 ****************  E G R E S S   P R O C E S S I N G   *******************
 *************************************************************************/

action action_clear_udp_checksum(){
    modify_field(udp.checksum, 0);
}

table table_clear_udp_checksum {
                                  
    actions {                                                                   
        action_clear_udp_checksum;                                  
    }                                                                           
    default_action : action_clear_udp_checksum;                 
    size : 1;                                                                   
}


action action_set_dst_mac_address(dst_mac_address) {     
    modify_field(ethernet.dstAddr, dst_mac_address);
}

table table_set_dst_mac_address {
    reads{
        eg_intr_md.egress_port : exact;
    }              
    actions {                                                                   
        action_set_dst_mac_address;                                  
    }                                                                           
    size : 256;                                                                   
}

control egress {
    if(valid(p4_scheduler)){                                        // Our Protocol
        apply(table_clear_udp_checksum);

        apply(table_read_and_write_register_register_functions_ids);
        apply(table_read_and_write_register_register_functions_parameters);
        apply(table_read_and_write_register_register_tasks_ids);
        apply(table_read_and_write_register_register_users_ips);
        apply(table_read_and_write_register_register_users_ports);

        apply(table_read_and_write_register_register_users_ids);
        apply(table_read_and_write_register_register_jobs_ids);
        apply(table_read_and_write_register_register_beggining_scheduling_time_1);
        // apply(table_read_and_write_register_register_beggining_scheduling_time_2);

        // Last steps
        if (p4_scheduler.operation_code == RETRIEVE_TASK_OPERATION_CODE){
            // if retrieve task == 0 > Reflect an empty packet or else populate packet, and send to sender
            apply(table_populate_retrieve_task_packet);
        }
       else if (p4_scheduler.operation_code == SUBMIT_JOB_OPERATION_CODE){
            // if # of remaining tasks = 0 >> drop
            apply(table_submit_next_task);
        }


    }
    else{                                                           // Other Protocols

    }                                            
    apply(table_set_dst_mac_address);                               // set dst mac address
}

///////////////////////////////////////////////////////////////////////////
