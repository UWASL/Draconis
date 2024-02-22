# Draconis

This repository contains both the P4 code for Draconis as well as the supporting DPDK code. The P4 code provided supports the FCFS scheduling policy.

Please cite our paper from EuroSys 2024 if you're using the code from this repository:

```
[1] Sreeharsha Udayashankar, Ashraf Abdel-Hadi, Ali Mashtizadeh, and Samer Al-Kiswany. 2024. Draconis: Network-Accelerated Scheduling for Micro-Scale Workloads. In Proceedings of The European Conference on Computer Systems (EuroSys ’24).
```

# System Properties and Requirements

The machines we ran our experiments on had the following software configuration:
  - Ubuntu 16.04.5 with Linux v4.4.0-210-generic
  - Intel DPDK v18.11
 
Our Barefoot Tofino switch controller had the following configuration:
  - Ubuntu v16.04.5 with Linux v4.4.0-131-generic
  - Barefoot SDK with P4 v14

# Setting up Draconis 

After installing the requirements specified above, the steps to deploy Draconis are as follows:

## Draconis - Switch Code
Copy the files within Draconis_P4\ onto the switch and run:
```
        cd $BF_SDE_DIR
        p4_build.sh --no_graphs p4scheduler.p4
```
### Modifying controller.txt
The controller rules may need to be modified to match the switch ports, MAC addresses and IP addresses of the target cluster. The specific rule-sets which need to be modified in "controller.txt" are:

_IPv4 Forwarding Rules_.
These rules indicate which switch physical egress ports need to be used to send packets with a certain destination IP address.
```
  pd table_layer_3_switching add_entry action_layer_3_switching ipv4_dstAddr <ipv4_destination_address> action_port <switch_egress_port_to_forward_packet>
```

_Destination MAC Addresses_.
These rules indicate the destination MAC address to be set for th epacket, based on which switch physical egress port the packet is to be sent out on
```
  pd table_set_dst_mac_address add_entry action_set_dst_mac_address eg_intr_md_egress_port <switch_port_number> action_dst_mac_address <mac_address_in_hex>
```

## Draconis - Workers, Clients and Draconis-DPDK-Server
This section applies to the deployment of the DPDK Backend, Client as well as Draconis-DPDK-Server. The Makefile provided within *Draconis_DPDK/* has sample commands to build each of these applications. After copying over the folder to the target machine, modify the Makefile to uncomment the relevant lines and then run the following commands:

        export $RTE_SDK=$PATH_TO_DPDK_18
        sudo -E make

This should build the relevant Draconis binary (depending on the Makefile modifications) within *Draconis_DPDK/build/*. Please note that the SCHEDULER_IP parameter within draconis_util.h may need to be modified as well to match the target cluster configuration.

# Running Draconis

### Scheduler

If Draconis-P4 is being used, use the following commands on the switch Linux:

        ./run_switchd.sh -p p4scheduler &
        ./run_bfshell.sh -f controller.txt

If using the Draconis-DPDK-Server instead of the P4 code, use the following command on one machine:
        
        sudo ./DraconisDPDKServer.o
        
Note that it is advised to run Draconis-DPDK-Server on all available cores for maximum performance.

### Backend Workers

To run a Draconis backend worker on any machine, use the following command:

        sudo ./DraconisBackend.o <time_to_run_in_sec> -l <cores>
        Example: sudo ./DraconisBackend.o 100 -l 1-16
        

### Clients and Workload Submission
Finally, to run the Draconis client,

      sudo ./DraconisClient.o <workload_file> <time_to_Wait_after_submissions_sec> <result_prefix>
      Example: sudo ./DraconisClient.o Exp_Mean_250us 30 Exponential_Workload_Results

#### Workload File
The workload file is in CSV format and has the following structure on each line:
  
      Job_ID,Job_Arrival_Time_Us,Task_Runtime_Us,Num_Tasks

Job ID has to be an integer for now and the times are in microseconds.

# DOI
The DOI for this code repository is:
```
10.5281/zenodo.10688914
```

