# Draconis
Centralized Network Accelerated Scheduling

This repository contains both the P4 code for Draconis as well as the supporting DPDK code. The P4 code provided uses a FCFS scheduling policy.

## System Properties and Requirements

The machines we ran our experiments on had the following software configuration:
  - Ubuntu 16.04.5 with Linux v4.4.0-210-generic
  - Intel DPDK v18.11
 
Our Barefoot Tofino switch controller had the following configuration:
  - Ubuntu v16.04.5 with Linux v4.4.0-131-generic
  - Barefoot SDK with P4 v14

## Deploying Draconis

After installing the requirements specified above, the steps to deploy Draconis are as follows:

### Switch Code Deployment
        cd $BF_SDE_DIR
        p4_build.sh --no_graphs p4scheduler.p4
        ./run_switchd.sh -p p4scheduler &
        ./run_bfshell.sh -f controller.txt

The controller may need to be modified to match the switch ports, MAC addresses and IP addresses of the target cluster

### DPDK Deployment

This section applies to the deployment of the DPDK Backend, Client as well as Draconis-DPDK-Server. The Makefile provided within Draconis_DPDK/ has sample commands to build each of these applications. After copying over the folder to the target machine, modify the Makefile to uncomment the relevane lines and then run the following commands:

        export $RTE_SDK=$PATH_TO_DPDK_18
        sudo -E make

This should build the relevant Draconis binary (depending on the Makefile modifications) within Draconis_DPDK/build/


