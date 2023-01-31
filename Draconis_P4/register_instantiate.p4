#define REGISTER_OPERATION_READ  0
#define REGISTER_OPERATION_WRITE 1
#define REGISTER_OPERATION_INC 	 2

// Creates a register 
#define create_register(register_name, entry_size, num_of_entries)									\
	register register_name {																		\
	    width : entry_size ;																		\
	    instance_count : num_of_entries;															\
	}																								\


// Counter read and increment
#define read_then_increment_register(register_name, index, read_destination)		\
		blackbox stateful_alu read_then_increment_register_##register_name {		\
	    reg: register_name;															\
	    output_predicate      : true;                   							\
	    output_dst            : read_destination;   								\
	    output_value          : register_lo;           								\
	    update_lo_1_value     : register_lo + 1;									\
	}																				\
	action action_read_then_increment_register_##register_name() {    				\
    	read_then_increment_register_##register_name.execute_stateful_alu(index);     \
	}																				\
	table table_read_then_increment_register_##register_name {						\
	    actions {																	\
	        action_read_then_increment_register_##register_name;						\
	    }																			\
    	default_action : action_read_then_increment_register_##register_name;			\
    	size : 1;																	\
	}																				\

// Writes values to register
#define write_register(register_name, index, value)									\
		blackbox stateful_alu write_register_##register_name {					\
	    reg: register_name;															\
	    update_lo_1_value     : value;									\
	}																				\
	action action_write_register_##register_name() {    						\
    	write_register_##register_name.execute_stateful_alu(index);     		\
	}																				\
	table table_write_register_##register_name {								\
	    actions {																	\
	        action_write_register_##register_name;								\
	    }																			\
    	default_action : action_write_register_##register_name;					\
    	size : 1;																	\
	}																				\

// Read without increment
#define read_register(register_name, index, read_destination)		\
		blackbox stateful_alu read_register_##register_name {		\
	    reg: register_name;															\
	    output_predicate      : true;                   							\
	    output_dst            : read_destination;   								\
	    output_value          : register_lo;           								\
	}																				\
	action action_read_register_##register_name() {    				\
    	read_register_##register_name.execute_stateful_alu(index);   \
	}																				\
	table table_read_register_##register_name {						\
	    actions {																	\
	        action_read_register_##register_name;					\
	    }																			\
    	default_action : action_read_register_##register_name;		\
    	size : 1;																	\
	}																				\





#define update_register_current_writes_hash()										\
	apply(table_write_register_register_current_writes_hash_1);						\
	apply(table_write_register_register_current_writes_hash_2);						\
	apply(table_write_register_register_current_writes_hash_3);						\
	apply(table_write_register_register_current_writes_hash_4);						\
	apply(table_write_register_register_current_writes_hash_5);						\
	apply(table_write_register_register_current_writes_hash_6);						\
	apply(table_write_register_register_current_writes_hash_7);						\
	apply(table_write_register_register_current_writes_hash_8);						\


#define read_register_current_writes_hash()										\
	apply(table_read_register_register_current_writes_hash_1);						\
	apply(table_read_register_register_current_writes_hash_2);						\
	apply(table_read_register_register_current_writes_hash_3);						\
	apply(table_read_register_register_current_writes_hash_4);						\
	apply(table_read_register_register_current_writes_hash_5);						\
	apply(table_read_register_register_current_writes_hash_6);						\
	apply(table_read_register_register_current_writes_hash_7);						\
	apply(table_read_register_register_current_writes_hash_8);						\



#define create_read_only_register(register_name, entry_size, num_of_entries, index, read_destination)						\
	register register_name {																								\
	    width : entry_size ;																								\
	    instance_count : num_of_entries;																					\
	}																														\
	blackbox stateful_alu read_register_##register_name {																	\
	    reg: register_name;																									\
	    output_predicate      : true;                   																	\
	    output_dst            : read_destination;   																		\
	    output_value          : register_lo;           																		\
	}																														\
	action action_read_register_##register_name() {    																		\
    	read_register_##register_name.execute_stateful_alu(index);   														\
	}																														\
	table table_read_register_##register_name {																				\
	    actions {																											\
	        action_read_register_##register_name;																			\
	    }																													\
    	default_action : action_read_register_##register_name;																\
    	size : 1;																											\
	}																														\

#define create_increment_counter_register(register_name, entry_size, num_of_entries, index, read_destination)				\
	register register_name {																								\
	    width : entry_size ;																								\
	    instance_count : num_of_entries;																					\
	}																														\
	blackbox stateful_alu read_then_increment_register_##register_name {												\
	    reg: register_name;																									\
	    output_predicate      : true;                   																	\
	    output_dst            : read_destination;   																		\
	    output_value          : register_lo;           																		\
	    update_lo_1_value     : register_lo + 1;																			\
	}																														\
	action action_read_then_increment_register_##register_name() {    														\
    	read_then_increment_register_##register_name.execute_stateful_alu(index);     										\
	}																														\
	table table_read_then_increment_register_##register_name {																\
	    actions {																											\
	        action_read_then_increment_register_##register_name;															\
	    }																													\
    	default_action : action_read_then_increment_register_##register_name;												\
    	size : 1;																											\
	}																														\


#define create_read_and_write_register(register_name, entry_size, num_of_entries, index, read_destination, write_data, will_write_field)    \
	register register_name {																												\
	    width : entry_size ;																												\
	    instance_count : num_of_entries;																									\
	}																																		\
	blackbox stateful_alu read_register_##register_name {																					\
	    reg: register_name;																													\
	    output_predicate      : true;                   																					\
	    output_dst            : read_destination;   																						\
	    output_value          : register_lo;           																						\
	}																																		\
	blackbox stateful_alu read_then_write_register_##register_name {																		\
	    reg: register_name;																													\
	    output_predicate      : true;                   																					\
	    output_dst            : read_destination;   																						\
	    output_value          : register_lo;           																						\
	    update_lo_1_value     : write_data;																									\
	}																																		\
	action action_read_register_##register_name() {    																						\
    	read_register_##register_name.execute_stateful_alu(index);     																		\
	}																																		\
	action action_read_then_write_register_##register_name() {    																			\
    	read_then_write_register_##register_name.execute_stateful_alu(index);     															\
	}																																		\
	table table_read_and_write_register_##register_name {																					\
		reads{																																\
			will_write_field : exact;																										\
		}																																	\
	    actions {																															\
	        action_read_register_##register_name;																							\
	        action_read_then_write_register_##register_name;																				\
	    }																																	\
    	size : 2;																															\
	}																																		\
	


#define create_read_and_write_and_inc_register(register_name, entry_size, num_of_entries, index, read_destination, write_data, operation)    \
	register register_name {																												\
	    width : entry_size ;																												\
	    instance_count : num_of_entries;																									\
	}																																		\
	blackbox stateful_alu read_register_##register_name {																					\
	    reg: register_name;																													\
	    output_predicate      : true;                   																					\
	    output_dst            : read_destination;   																						\
	    output_value          : register_lo;           																						\
	}																																		\
	blackbox stateful_alu read_then_write_register_##register_name {																		\
	    reg: register_name;																													\
	    output_predicate      : true;                   																					\
	    output_dst            : read_destination;   																						\
	    output_value          : register_lo;           																						\
	    update_lo_1_value     : write_data;																									\
	}																																		\
	blackbox stateful_alu read_then_inc_register_##register_name {																			\
	    reg: register_name;																													\
	    output_predicate      : true;                   																					\
	    output_dst            : read_destination;   																						\
	    output_value          : register_lo;           																						\
	    update_lo_1_value     : register_lo + 1;																							\
	}																																		\
	action action_read_register_##register_name() {    																						\
    	read_register_##register_name.execute_stateful_alu(index);     																		\
	}																																		\
	action action_read_then_write_register_##register_name() {    																			\
    	read_then_write_register_##register_name.execute_stateful_alu(index);     															\
	}																																		\
	action action_read_then_inc_register_##register_name() {    																			\
    	read_then_inc_register_##register_name.execute_stateful_alu(index);     															\
	}																																		\
	table table_read_and_write_and_inc_register_##register_name {																					\
		reads{																																\
			operation : exact;																												\
		}																																	\
	    actions {																															\
	        action_read_register_##register_name;																							\
	        action_read_then_write_register_##register_name;																				\
	        action_read_then_inc_register_##register_name;																					\
	    }																																	\
    	size : 3;																															\
	}																																		\
