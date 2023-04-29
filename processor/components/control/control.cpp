#include "systemc.h"

/* Component: Control module
 *
 * Control module of the processor. Responsible
 * for activating/deactivating functions of
 * the other components at each cycle.
 *
 * Implements internally a Mealy's finite state machine.
 * **/
SC_MODULE(control) {

	public:
		//-- Signals --//
		//-- In --//
		sc_in_clk clock;
		sc_in<sc_uint<4> > opcode;	// OPCODE
		sc_in<sc_uint<8> > of1;		// Operando fonte 1
		sc_in<sc_uint<8> > of2;		// Operando fonte 2
		sc_in<sc_uint<9> > opd;		// Operando destino
		sc_in<bool> N;
		sc_in<bool> Z;

		//-- Out --//
		// Program counter control
		sc_out<bool> enableCP;
		sc_out<bool> loadCP;
		sc_out<bool> resetCP;
		sc_out<sc_uint<9>> jumpValueCP;

		// Instruction memory control
		sc_out<bool> enableIM;
		sc_out<bool> writeIM;

		// Data memory control;
		sc_out<bool> enableDM;
		sc_out<bool> writeDM;

		// Instruction register control
		sc_out<bool> enableRI;
		sc_out<bool> writeRI;
		sc_out<sc_uint<8>> immediateValue;
		sc_out<sc_uint<9>> immediateRegister;

		// Register base control
		sc_out<bool> enableRB;
		sc_out<bool> writeRB;

		// Register pipeline
		sc_out<bool> enableRPL;
		sc_out<bool> writeRPL;
		sc_out<bool> resetRPL;

		// ULA control
		sc_out<bool> resetZN;

		// Multiplex for write data on register base
		sc_out<sc_uint<2>> seletorMultiRBW;

		// Multiplex for addressing DM
		sc_out<sc_uint<2>> seletorMultiDM;
	
		//-- Constructor --//
		SC_CTOR(control) {
			SC_METHOD(state_machine);
			sensitive << clock.pos();
		}	

        unsigned getCycleCount() const { return cycleCount; }

	private:
		//-- Local variables --//
        void (control::*state)() = &control::state_READ_IM;
		bool restartPipe = false;
        unsigned cycleCount = 0;

		//-- Methods for making pipeline easy --//

		/*
		 * Prepare components so that, in the next cycle,
		 * the instruction is available on the bus.
		 * * */
		void prepareInstToBus();

		/* Prepare components so that the instruction
		 * is stored in the instruction register in
		 * the next cycle.
		 * * */
		void prepareInstBusToRI();

		/* Prepare components so that the decodified instruction
		 * is written in the pipeline register.
		 * **/
		void prepareRItoRPL();

		/* Make the instruction waits when reaches the cycle
		 * before the pipeline register (for pipeline purposes
		 * only).
		 * **/
		void stopPipePropagation();

        void prepareJump();
        void prepareConditionalJump();

		/* The Mealy's state machine: uses the current
		 * state and informations from outside in order
		 * to keep the processor working properly in
		 * terms of microinstructions.
		 * **/
		void state_machine();

        void state_READ_IM();
        void state_LOAD_IR();
        void state_LOAD_RPL();
        void state_READ_NEXT_INST();
        void state_PREP_EXECUTION();
        void state_EXEC_ALU();
        void state_EXEC_STORE();
        void state_EXEC_JUMP();
        void state_STORE_ALU_RESULTS();
        void state_EXEC_LOAD();

};

void control::state_machine() {
    ++cycleCount;
    (*this.*state)();
}

void control::state_READ_IM() {
    prepareInstToBus();
    state = &control::state_LOAD_IR;
}

void control::state_LOAD_IR() {
    prepareInstBusToRI();
    state = &control::state_LOAD_RPL;
}

void control::state_LOAD_RPL() {
    if (restartPipe) {
        restartPipe = false;
        state = &control::state_READ_IM;
    } else { 
        prepareRItoRPL();
        state = &control::state_READ_NEXT_INST;
    }
}

void control::state_READ_NEXT_INST() {
    enableRPL.write(0);
    prepareInstToBus(); // Take new instruction (pipeline)
    state = &control::state_PREP_EXECUTION;
}

void control::state_PREP_EXECUTION() {
    prepareInstBusToRI(); // Put new instruction on RI (pipeline)
    
    // Load operations: can be LRI (immediate) or LD
    if (opcode.read() == 8 || opcode.read() == 13) {
        enableRB.write(1);
        writeRB.write(1);
        // LRI operation
        if (opcode.read() == 13) {
            immediateRegister.write(opd.read());
            immediateValue.write(of1.read());
            seletorMultiRBW.write(2);
            state = &control::state_STORE_ALU_RESULTS;
        // LD operation
        } else if (opcode.read() == 8) {
            enableDM.write(1);
            writeDM.write(0);
            seletorMultiRBW.write(1);
            seletorMultiDM.write(1);
            state = &control::state_EXEC_LOAD;
        }
    // ST operation
    } else if (opcode.read() == 9) {
        enableRB.write(1);
        writeRB.write(0);
        state = &control::state_EXEC_STORE;
        seletorMultiDM.write(0);
    // J operation
    } else if (opcode.read() == 10) {
        prepareJump();
        restartPipe = true;
        state = &control::state_EXEC_JUMP;
    // JN operation
    } else if (opcode.read() == 11) {
        if (N.read() == 1) {
            prepareConditionalJump();
            restartPipe = true;
        }
        state = &control::state_EXEC_JUMP;
    // JZ operation
    } else if (opcode.read() == 12) {
        if (Z.read() == 1) {
            prepareConditionalJump();
            restartPipe = true;
        }
        state = &control::state_EXEC_JUMP;
    // ULA operations
    } else if (opcode.read() != 0) {
        seletorMultiRBW.write(0);
        enableRB.write(1);
        writeRB.write(0);
        state = &control::state_EXEC_ALU;
    } else if (opcode.read() == 0) {
        sc_stop();
    }
}

void control::state_EXEC_ALU() {
    enableRB.write(1);
    writeRB.write(1);
    stopPipePropagation();
    state = &control::state_STORE_ALU_RESULTS;
}

void control::state_EXEC_STORE() {
    enableDM.write(1);
    writeDM.write(1);
    stopPipePropagation();
    state = &control::state_STORE_ALU_RESULTS;
}

void control::state_EXEC_JUMP() {
    loadCP.write(0);
    state = &control::state_LOAD_RPL;
}

void control::state_STORE_ALU_RESULTS() {
    enableRB.write(0);
    enableDM.write(0);
    stopPipePropagation();
    state = &control::state_LOAD_RPL;
}

void control::state_EXEC_LOAD() {
    enableRB.write(1);
    writeRB.write(1);
    state = &control::state_STORE_ALU_RESULTS;
}

void control::prepareInstToBus() {
	enableIM.write(1);	// Enable IM
	writeIM.write(0);	// Read from IM
	enableCP.write(1);	// Increment counter
}

void control::prepareInstBusToRI() {
	enableIM.write(0);	// Disable IM
	enableRI.write(1);	// Enable RI	
	writeRI.write(1);	// Write IR
	enableCP.write(0);	// Stop incrementing PC
}

void control::prepareRItoRPL() {
	enableRI.write(0);	// Disable Ri
	enableRPL.write(1);	// Enable pipeline register 
	writeRPL.write(1);	// Write pipeline register
}

void control::stopPipePropagation() {
	enableRI.write(0);	
}

void control::prepareJump() {
    enableCP.write(0);
    loadCP.write(1);
    jumpValueCP.write(opd);    
}

void control::prepareConditionalJump() {
    prepareJump();
    resetZN.write(1);
}