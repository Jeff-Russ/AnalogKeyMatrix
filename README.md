# AnalogKeyMatrix Driver C++ Class Library for Arduino

AnalogKeyMatrix allows you to read multiple keys (momentary switches) from a 
key matrix outputing a single voltage to an Arduino analog input pin easily, 
to be interpreted as "press / release", "double-click", "long-press" patterns.  

Although designed for number pad key matrixes, it works with any source of 
voltage input that is divided into any number of parts (number of keys or buttons)
plus an OFF state which could be VCC or GND or close to them. This flexibility 
allows it to be used with devices such as keypads, keyboards and even musical CV keyboards.  

The constructor is somewhat complex and should be called during setup whereas the 
methods called afterwards are quite light-weight for fast performance.  

## Development Status

This is my current active project and, although it appears to function smoothly, 
it's early enough that I'm sure there are some bugs that will be ironed out in the 
next few weeks. Go ahead and download it but check for updates soon thereafter!  

## AnalogKeyMatrix Constructor Arguments and it's Defaults

	AnalogKeyMatrix my_keymatrix (
		uint8_t pin,
		uint8_t numOfKeys = AKM_defaultNumOfKeys, 
		uint16_t keyVals[AKM_defaultNumOfKeys] = AKM_defaultKeyVals,
		short releaseState = -1,
		double releaseTol = 0.1
	);

__1st Constructor Argument: `uint8_t pin`__  
  
Is the pin number your object will use to call `analogRead()`. It is the only 
required argument.  

__2nd Constructor Argument: `uint8_t numOfKeys`__   

This is the size of the 3rd argument array. It's default is:  

	uint8_t AKM_defaultNumOfKeys = 12;

__3nd Constructor Argument: `uint16_t keyVals[]`__   
  
The third argument Is an array of approximate `analogRead()` return values for each key in your input and the second argument is the size of that array. The values need not be 
in order so that it might be indexed in a way that's meaningful to you. For example, 
with a number pad you would want the zero (0) key to be index `0` even if other keys have lower voltage output. For non-numeric keys with names you may define an enum or similar for readability. The array does, however, have to constitute a valid C array, indexed 0 to size-1).  

Here is the defaults set in the library, which can be overwritten.  It is a 
numeric keypad:

	uint16_t AKM_defaultKeyVals[12] = {
		510, // 0 key
		1023, 930, 850, // 1, 2, 3
		790, 730, 680,  // 4, 5, 6
		640, 600, 570,  // 7, 8, 9
		540, 490 // bottom row left and right of 0 key
	};

__4th Constructor Argument: `short releaseState`__  

The fourth argument specifies whether GND or VCC is received when keys all are 
released. Set it to `HIGH` for VCC, `LOW` for GND or `-1` (the default value) 
to have it be determined automatically from the values of `keyVals[]`. If a `0`
 is found, release is `HIGH` and if `1023` is found, it's `LOW`.  

__5th Constructor Argument: `double releaseTol`__  

The key values don't need to be exact since a `min` and `max` will be calculated
during the constructor as halfway between each value you specify in the `keyVals[]` third argument. That halfway point might not be halfway for the `min` of the lowest 
value in the case of release being GND... or `max` of the top value when release is 
`HIGH`. It will be if the third argument, `releaseTol` is set to `0.5`. With a 
very low release tolerance of, say 0.1, It might mean that the `min` for the 
lowest key is `1` and release is only `0` from `analogRead()`, for example.  

The default is `0.1` which leave a _little_ room for the release state and more 
room for the lowest or highest key than any other key.  

## Feching Device State via .read() Method and the AKM_State struct Pointers

After you construct your object... 

	AnalogKeyMatrix my_keymatrix(A0);  
	
You can call .read()  

	my_keymatrix.read();

__Data Member: my_keymatrix.log[] array of AKM_State struct pointers__  

After calling `.read()`, will have three data members of concern to you that will be updated. `my_keymatrix.log` is an array of pointers to the current and previous 
states read. Each element is a pointer to a struct of the type `AKM_State`. 
The most recent is always the zeroth element, the previous is the next element
and least recent is the last element.  

__Data Members: my_keymatrix.now and my_keymatrix.now AKM_State struct pointers__  

Typically you would be concerned with the most recent and previous reading, 
which have their own data member `.now` and `.prev`, which are just pointers to 
the same data. This shows the fields in all of these `AKM_State` structs:  


	my_keymatrix.now->val;    // int return from analogRead(), always updated
	my_keymatrix.now->event;  // int, see AKM_* event statuses 
	my_keymatrix.now->key;    // int key number or -1 if idle
	my_keymatrix.now->ms;     // unsigned long from millis()

__Reading directly from return of `.read()` Method__  
	
`.read()` returns a pointer to the `AKM_State now` struct, so you could save the 
output from `.read()` to a variable (`AKM_State * now = my_keymatrix.read()`) or, 
if you only need to look at one field, you can grab it from the call to `.read()`:  

	if (my_keymatrix.read()->event == ...

Which might be quicker. Also note that you can read the same data with the `.log` 
data member:  

	// .log[0] same as .now ...
	my_keymatrix.log[0]->event;
	my_keymatrix.log[0]->event; // etc...

	// .log[1] same as .prev ...
	my_keymatrix.log[1]->val; 
	my_keymatrix.log[1]->event; // etc... 

## AKM_* Events Statuses and how .read() edits the .log

If `.read()` found a key is currently pressed down, my_keymatrix.now->event 
will be `2` (or possibly greater of class is extended). If not, it is less 
than `2`.  You can compare it's value to these macros defined in this class 
library:  

	#define AKM_IDLE    0
	#define AKM_RELEASE 1
	#define AKM_PRESS   2

The `AKM_RELEASE` status only happens once after a press, then the next 
status and those after will be `AKM_IDLE` if no subsequent key press is 
found.  

__ AKM\_PRESS status__  

	my_keymatrix.now->val;    // => int from analogRead() IN range for ->key
	my_keymatrix.now->event;  // => AKM_PRESS
	my_keymatrix.now->key;    // int key number (never -1)
	my_keymatrix.now->ms;     // millis() at FIRST in series of AKM_PRESS

When the object first finds any key is pressed, a new event slot in `.log[]` 
is written to as seen above with ->now pointing to it. `->ms` is updated 
but if the next reading also shows a key pressed (even a different key), 
`->ms` remains as it was. In fact, `->now` is simply written over in this 
case, with updated `->val` and `->key`.  If you want to see how long it's 
been pressed, compare `->ms` (the initial time AKM_PRESS occurred) with 
your own call to `.millis()`.  

__ AKM\_RELEASE status__ 

	my_keymatrix.now->val;    // => analogRead() int OUT of range of any key
	my_keymatrix.now->event;  // => AKM_RELEASE
	my_keymatrix.now->key;    // int key number of previous event (.prev->key)
	my_keymatrix.now->ms;     // millis() at FIRST in series of AKM_RELEASE

When the key is finally released, the log advances to a new AKM_State slot 
position and makes the event `AKM_RELEASE`. A new timestamp and a new value are 
recorded but `->key` will be the __last key pressed__ so you can see what 
key was just released. You can compare the two timestamps to see how long it 
was pressed before being released.  


# Advanced Useage

### Modifier Keys (Overlapped Key Presses)

Although the AnalogKeyMatrix doesn't have a direct way to determine if two keys are 
pressed at the same time, you can still determine this but going back in the 
`.log`.  

As said previously, __a second reading of the same key being pressed 
does not make a new event BUT two consecutive readings of DIFFERENT keys 
does__. The second key press is logged just like the first, with a new slot 
and all fields updated. When a `AKM_RELEASE` is found, you can check the past two 
events to see if they are both `AKM_PRESS`. The time between each can be determined 
with the three `->ms`.  

You can even check for more than one modifier by by going back further in the 
log (see 'Other Global Setup Constants' section for limitations).

### Long Press Detection

If you want to see if a key is pressed for over a certain threshold time, 
look for a `AKM_PRESS` for a particular key and keep comparing it's `->ms`, 
which not updated and therefore just the time the key was first pressed, 
to the current return from `.millis()`.  

### AKM_IDLE and timeouts

After `AKM_RELEASE` is logged (if the next reading doesn't show another key 
pressed) the `.now->event` will be `AKM_IDLE`. All fields including `->val` and 
`->ms` are updated. `->key` will be `-1` indicating no key (and not a valid
array index). So if you are checking for a particular key, you can skip the 
step of looking for `->event` and just look at `->key`. This is helpful if you 
are looking for __Long Press__ since, in that case, you are probably not waiting 
until you see it released.  

Also if you are detecting a timeout where the user has not pressed anything for 
a period of time, know that `AKM_IDLE` does not get updated with a new `->ms` 
timestamp. In fact, nothing in `.log` or related members are changed, the same 
`.now` pointer is repeatedly returned from `.read()`. Therefore, your method of 
timing the inactivity should be comparing this `->ms` as a start time to whatever 
return you get from `.millis()`.  

# Default Startup States and Detecting Special Startup Key(s) Held

When the constructor is called the entire `.log` array is filled with some readings 
and default values. All elements will show these values:  

	->val    // return from a single call to analogRead()
	->event  // usually AKM_IDLE but could be AKM_PRESS
	->key    // usually -1 (if idle) but could be a key number
	->ms     // return from a single call to millis()

It is possible, if the constructor is called very early, that Arduino will 
return a junk value from some functions so you may need to be wary in assuming 
a key is held down. You can, of course, call `.read()` several time, which would 
be absolutely necessary if you want to check for multiple keys being held.  

## Other Global Setup Constants via `#define`

As mentioned, AKM_defaultNumOfKeys and AKM_defaultKeyVals[AKM_defaultNumOfKeys] are 
variable that can be written to before calling the constructor. There are also 
two other values which are macros in the AnalogKeyMatrix library that you may want to 
change. Here they are as defined in the library:  

	#ifndef AKM_MaxNumOfKeys
	  #define AKM_MaxNumOfKeys 48 // maximum size of AKM_defaultKeyVals[] supported
	#endif
	
	#ifndef AKM_LogSize
	  #define AKM_LogSize 6       // size of .log[] array
	#endif

These are purposely quite high. AKM_LogSize of 6 allows for 3 keys to be pressed, 
then released. And AKM_MaxNumOfKeys 48 allows for a four octave CV keyboard or an 
abbreviated alpha-numeric keyboard. But not using all of that takes a bit of 
memory and, in the case of AKM_LogSize, a bit of extra clock CPU clock cycles 
during `.read()`. 
