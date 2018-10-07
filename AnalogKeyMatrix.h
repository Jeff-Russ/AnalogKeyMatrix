// AnalogKeyMatrix.h

////////////////////////////////////////////////////////////////////////////////
// #define DEBUG_ON

#ifdef DEBUG_ON
  #define IF_DEBUG(x) x
#else 
  #define IF_DEBUG(x)
#endif

// #define VERBOSE_ON

#ifdef VERBOSE_ON
  #define IF_VERBOSE(x) x
#else 
  #define IF_VERBOSE(x)
#endif

// #define AKM_ERROR -1 // analogRead returned out of bounds

#ifdef AKM_ERROR
  #define AKM_ERR(x) x
#else 
  #define AKM_ERR(x)
#endif

////////////////////////////////////////////////////////////////////////////////
/////////// Default values for AnalogKeyMatrix constructor arguments /////////////////

// indexes are key numbers and don't reflect order of values...
uint16_t AKM_defaultKeyVals[12] = {
	510, // 0 key is bottom row middle
	1023, 930, 850, // 1, 2, 3
	790, 730, 680,  // 4, 5, 6
	640, 600, 570,  // 7, 8, 9
	540, 490 // bottom row left and right
};
// to know size of array containing key values...
uint8_t AKM_defaultNumOfKeys = 12;

#ifndef AKM_MaxNumOfKeys
  #define AKM_MaxNumOfKeys 48 // maximum size of AKM_defaultKeyVals[] supported
#endif

#ifndef AKM_LogSize
  #define AKM_LogSize 6       // size of .log[] array
#endif

////////////////////////////////////////////////////////////////////////////////
/////////// AKM_State.event field values representing different events ///////////

	/* Below, states of 2 or greater indicate a key is pressed down. 
	 * States of 0 or less indicate no key is pressed. Exactly which 
	 * event registers will depend the prev(ious) event, if any (it could  
	 * be the first call) stored. A new state is made ONLY for a new value 
	 * of AKM_State.key so AKM_RELEASE changes to AKM_IDLE and AKM_PRESS and 
	 * AKM_OVERLAP remain as such. THe only thing that changes is AKM_State.msHold
	 */
// #define AKM_ERROR  -1 // analogRead returned out of bounds (enabled?)
#define AKM_IDLE    0 // no key pressed now and prev <= AKM_RELEASE
#define AKM_RELEASE 1 // prev >= AKM_PRESS (key pressed or held)
#define AKM_PRESS   2 // any key is pressed and prev <= AKM_RELEASE
// #define AKM_OVERLAP 3 // state and prev >= AKM_PRESS for DIFFERENT key

typedef struct AKM_State AKM_State;
struct AKM_State {
	int val;    // integer return from analogRead() stored for posterity
	int event;  // see #define AKM_* macros 
	int key; // key number or -1 if idle
	unsigned long ms; // see below
};
/* millis() is only logged when key matrix FIRST goes from OFF to a key 
 * being pressed and, when FIRST returning to an OFF state (AKM_PRESS goes
 * to AKM_RELEASE) and when AKM_RELEASE goes to AKM_IDLE.
 */

/*
	*****************************************************************************
	TODO:
	1.
	Make an option to log duplicate ON states with slightly different values 
	(helpful if a pot is involved). This, of course, requires much more than 6 
	(the default set by #define AKM_LogSize) slots, maybe a 3 dim array.

	2. Make methods for isLongHold, isDoublePress and getModifiers which analyzes 
	current and previous state(s),
	*****************************************************************************
*/


class AnalogKeyMatrix
{
  public: // #### COMMENT THIS OUT #####
	uint8_t pin, // #### MAKE THIS PUBLIC #####
		numOfKeys, // usually up to 48 (see #define AKM_MaxNumOfKeys)
		nthKey;    // index of last key in this->keys
	uint16_t keys[AKM_MaxNumOfKeys][4];
		// In this->keys[x][y], x is value (sorted) and y? see below...
	enum keyIndexes{ knum = 0, kval = 1, kmin = 2, kmax = 3 };
	
	// this is array of the current and previous states and optionally more
	// (total is set by #define AKM_LogSize, which must not be less than 2!).
	// It's written in a circular mannner ( [0] is not always first in time )
	// At the object level, it will only be READ via pointers. 
	AKM_State _log[AKM_LogSize];    // array current + 1 or more previous states
	uint8_t now_i; // position of current state in _log[]
	uint8_t prev_i; // position of previous state in _log[]
		
  public:
	
	// these are public for being read outside, not to be modified via object:
	AKM_State * log[AKM_LogSize]; // pointers to this->_log, always in order
	AKM_State * now;    // = &this->_log[ this->now_i ]
	AKM_State * prev;   // = &this->_log[ this->prev_i]

	AnalogKeyMatrix (
		uint8_t pin,
		uint8_t numOfKeys = AKM_defaultNumOfKeys, 
		uint16_t keyVals[AKM_MaxNumOfKeys] = AKM_defaultKeyVals,
		short releaseState = -1, // LOW (0) or HIGH (0). With -1, determined by vals
		double releaseTol = 0.1 // 0.0 tolerance means only 0 or 1023 is considered released
	){
		this->pin = pin;
			pinMode(pin, INPUT);
			analogRead(this->pin); // clear out garbage right after startup

		// copy to 2-dim array, unsorted:
		uint8_t i;
		for (i = 0; i < numOfKeys; ++i) {
			// these will be moved to a different [i]{...} when sorted
			this->keys[i][knum] = i;          // key number
			this->keys[i][kval] = keyVals[i]; // key value
		}

		// sort each subarray by it's key value... [?][1]:
		uint16_t j, n; // j is i+1,
		uint16_t v; // v is value
		for (i = 0; i < numOfKeys; ++i) {
			for (j = i + 1; j < numOfKeys; ++j) {
				if (this->keys[i][kval] > this->keys[j][kval])  {
					// copy key number and value to correct position:
					n = this->keys[i][knum];
					v = this->keys[i][kval];
					this->keys[i][knum] = this->keys[j][knum];
					this->keys[j][knum] = n;
					this->keys[i][kval] = this->keys[j][kval];
					this->keys[j][kval] = v;
				}
			}
		}
		
		// determine of release it 0 or 1023:
		uint8_t nthKey = numOfKeys - 1;
		if (releaseState == -1)
			releaseState = (this->keys[0][kval] == 0) ? HIGH : LOW;
		else if (releaseState > 0) releaseState = HIGH;
		else releaseState = LOW;
			
		
		///// set limits for each key ////
		uint16_t temp, *currVal, *prevVal, *nextVal;

		// set key's min/max, skipping lowest key's min and highest's max
		for (i = 1; i < nthKey; ++i)
		{
			currVal = &this->keys[i][kval];
			prevVal = &this->keys[i-1][kval];
			
			// Each key's min value is halfway to above previous value. Fallback
			// checking temp's place in sequence to prevent errors of overlap.
			temp = *currVal - ( *currVal - *prevVal) / 2.0;
			if (temp <= *prevVal || temp > *currVal ) temp = *currVal; // fallback
			this->keys[i][kmin] = temp;

			// set previous key's max value to current's min - 1
			temp = this->keys[i][kmin] - 1;
			if (temp < *prevVal || temp >= *currVal) temp = *prevVal; // fallback
			this->keys[i-1][kmax] = temp;

			if (i == nthKey - 1)
			{
				nextVal = &this->keys[nthKey][kval];
				
				// set next (last) key's min value to halfway to above current value:
				temp = *nextVal - ( *nextVal - *currVal) / 2.0;
				if (temp <=*currVal || temp > *nextVal) temp = *nextVal; // fallback
				this->keys[nthKey][kmin] = temp;

				// set current key's max value to next's (last's) min - 1
				temp = this->keys[nthKey][kmin] - 1;
				if (temp <= *currVal || temp >= *nextVal ) temp = *currVal; // fallback
				this->keys[i][kmax] = temp;
			}
		}

		// taking tolerance and release as LOW or HIGH, set low botton's min + high's max:
		uint16_t releaseBP; // max or min value considered released.
		if (releaseState == HIGH) {
			this->keys[0][kmin] = 0;
			releaseBP = 1023 - ( (uint16_t)( (1023.0 - this->keys[nthKey][kmin]) * releaseTol ) );
			if (releaseBP >= 1023) releaseBP = 1023 - 1; // fallback
			this->keys[nthKey][kmax] = releaseBP;
		} else {
			this->keys[nthKey][kmax] = 1023;
			releaseBP = (uint16_t)( this->keys[0][kval] * releaseTol );
			this->keys[0][kmin] = releaseBP;
		}
		///// END set limits for each key ////
		
		this->numOfKeys = numOfKeys;
		this->nthKey = nthKey; // to be used later without this->numOfKeys - 1
		
		this->resetStates();
	}
	
	AKM_State * resetStates ()
	{
		IF_VERBOSE( Serial.println("\n====resetting states"); );
		int val = analogRead(this->pin);
		unsigned long ms = millis();

		for (uint8_t i = 0; i < AKM_LogSize; i++) // this->numOfKeys? no. AKM_LogSize
		{
			this->_log[i].val = val;
			this->_log[i].event = AKM_IDLE;
			this->_log[i].key = -1;
			this->_log[i].ms = ms;

			this->log[i] =& this->_log[i];
		}

		this->now_i = 0;
		this->now  =& this->_log[this->now_i];
		this->prev_i = AKM_LogSize - 1;
		this->prev =& this->_log[this->prev_i];
		
		if (val >= this->keys[0][kmin] 
		 && val <= this->keys[this->nthKey][kmax] ) {
			IF_VERBOSE( Serial.println("\n====resetStates() calling read()..."); );
			return this->read();
		} else {
			return this->now;
		}
	}
		
	////////////////////////////////////////////////////////////////////////////
	AKM_State * read()
	{
		IF_VERBOSE( Serial.println("\n====read()"); );
		int val = analogRead(this->pin);
		int event = -1;  // see #define AKM_* macros. defaulted to an error (-1)
		int key = -1; // key number or -1 if idle
		unsigned long ms = -1; // -1 if not needed, set to millis if it is.

		int this_i = this->now_i; // may or may not become (this_i + 1) % AKM_LogSize;
		AKM_State * last_call =& this->_log[this_i];
		int key_i; // for looping though keys
		
		IF_VERBOSE( Serial.println("value is "+String(val) 
			+ ", this->now_i is "+String(this->now_i) ); );

		/* determine key and event *****************************************/
		
		// OFF state (No keys pressed): 
		if (val < this->keys[0][kmin] 
		 || val > this->keys[this->nthKey][kmax]
		){
			AKM_ERR( 
			if (val < 0 || val > 1023) {
				last_call->val = val;
				last_call->event = AKM_ERROR;
				last_call->key = -1;
				last_call->ms = millis();
				return last_call;
			}
			);

			if (last_call->event == AKM_IDLE) {
				last_call->val = val;
				IF_VERBOSE( Serial.println("OFF: idle continues"); );
				return last_call;
			} else {
				this_i = (this_i + 1) % AKM_LogSize;
				ms = millis();

				if (last_call->event >= AKM_PRESS) {
					IF_VERBOSE(
						Serial.println("OFF: A key or multiple key's first release"); 
					);
					event = AKM_RELEASE;
					key = last_call->key;
				} else {
					IF_VERBOSE(
						Serial.println("OFF: first idle (last was release or error)");
					);
					event = AKM_IDLE;
					key = -1;
				}
			}
		} else {
			IF_VERBOSE( Serial.print("ON: A key is pressed, figure out which one..."); );
			bool found = false;
			for (key_i = 0; key_i < this->numOfKeys; key_i++) {
				if (val <= this->keys[key_i][kmax]) // a pressed key is found
				{
					key = this->keys[key_i][knum];
					IF_VERBOSE( Serial.println(String(key)); );
					if (last_call->event == AKM_PRESS && last_call->key == key) {
						IF_VERBOSE(
							Serial.println("ON: Same key pressed as last time: "
							+ String(last_call->key));
						);
						found = true;
						return last_call;
					} else {
						IF_VERBOSE(
							Serial.println(
							"ON: previously we had idle, a different key down, or error");
						);
						this_i = (this_i + 1) % AKM_LogSize;
						event = AKM_PRESS;
						// key = key_i; // WRONG
						ms = millis();
						found = true;
					}
				}
				if (found) break;
			}
		}
		
		if (this_i != this->now_i)
		{
			IF_VERBOSE( Serial.println("new index for ->now and ->prev. ->log shuffled"); );
			this->prev =& this->_log[this->now_i];
			this->now  =& this->_log[this_i];
			this->now_i = this_i;
			
			this->log[0] =& this->_log[this->now_i];

			uint8_t n = 0;
			int8_t nAgoIdx;
			for (n = 1; n < AKM_LogSize; n++) // this->numOfKeys? no. AKM_LogSize
			{
				// calc prev event's indexes, but wrapping from -1 to
				// last index, -2 to last index - 1, etc:
				nAgoIdx = (nAgoIdx = this->now_i - n) >= 0 ? 
					nAgoIdx : ( AKM_LogSize + nAgoIdx );
				// shuffle around pointers in this->log
				this->log[n] =& this->_log[nAgoIdx];
				if (n == 1) this->prev =& this->_log[nAgoIdx];
			}

		}
		// write to _log at this_i which might or might not have changed:
		this->_log[this_i].val = val;
		this->_log[this_i].event = event;
		this->_log[this_i].key = key;
		this->_log[this_i].ms = ms;
		
		return this->now;
	}

IF_DEBUG(
	void debugKeys() {
		for (uint8_t i = 0; i < this->numOfKeys; i++) {
			Serial.print("knum: " + String(this->keys[i][knum]) );
			Serial.print(" kval: " + String(this->keys[i][kval]) );
			Serial.print(" kmin: " + String(this->keys[i][kmin]) );
			Serial.println(" kmax: " + String(this->keys[i][kmax]) );
		}
	}
	void debugNow() {
		// val event key ms
		Serial.print("prev_i " + String(this->prev_i) );
		Serial.print(", now_i " + String(this->now_i) );
		Serial.print(": val " + String(this->now->val) );
		switch(this->now->event) {
		  case 0 :
			Serial.print(", AKM_IDLE"); break;
		  case 1 :
			Serial.print(", AKM_RELEASE"); break;
		  case 2 :
			Serial.print(", AKM_PRESS"); break;
		  default :
			Serial.print( String(this->now->event) );
		}
		Serial.print(", key " + String(this->now->key) );
		Serial.println(", " + String(this->now->ms) + "ms");
	}
	void debugStates() {
		// val event key ms
		Serial.println("\n====debugStates()");
		Serial.print("current state " + String(this->now_i) );
		Serial.println(", prev_i " + String(this->prev_i) );
		for (int i = 0; i < AKM_LogSize; i++) {
			AKM_State * s =& this->_log[i];
			Serial.print("state" + String(i) + ": val " + String(s->val) );
			Serial.print(", ");
			switch(s->event) {
			  case 0 :
				Serial.print("AKM_IDLE"); break;
			  case 1 :
				Serial.print("AKM_RELEASE"); break;
			  case 2 :
				Serial.print("AKM_PRESS"); break;
			  default :
				Serial.print( String(s->event) );
			}
			Serial.print(", key " + String(s->key) );
			Serial.println(", " + String(s->ms) + "ms");
		}
	}
);
};

