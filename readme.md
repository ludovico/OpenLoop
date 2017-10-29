### The OpenLoop project

The OpenLoop is an experimental project, where the Juce Framework is exposed through ~~Lua~~ ~~Chaiscript~~ ~~bindings~~ messages. 

The Juce Framework is a powerful c++ library for audio developers. You have access to the main audio callback that sends samples to the audio interface, you've got plugin support with parameter control, MIDI, and audio file support. All major formats are supported, on all platforms. It is GPL-licenced, which is a permitting licence - anyone can use the library and share it, as long as their own work is GPL.

JUCE was released in 2004, and is powerful enough that you can build a full-featured host on top of it, and distribute it for free as long as the code stays GPL. This is a gamechanger (for me!), because it enables lowly programmers (like me!) to 1: imagine a host and 2: create it.

The most typical host is a DAW. The interface usually resembles an analog studio where you can see and touch everything, has a sequencer and a timeline, and total recall. A less typical host is Max/MSP, which is a visual programming language. Here you connect boxes on a screen, and that is your program. It is a simplification of programming, which is great if you don't want to touch code but still experiment. 

I think there is room for a host where dynamic, text-based programming is the major mode of interaction. It would run against a backend which is responsible for the DSP processing (which is cpu-intensive and needs to be compiled). This split will exist for a long time, I don't see any serious DSP company moving away from C/C++ anytime soon.

The challenge is to design an API - mediating the boundary between the C++ backend and whatever frontend - that is as simple and expressive as possible. From the front-end perspective, which plugin format is irrelevant - either it has the capabilities (for sample-accurate parameters i.ex.) or not. Double vs float-processing should default to double. 

Originally I wanted to use a lisp-type of language, but Chaiscript was the easiest to bind - and the most important thing for now is to get some sort of dynamism up and running. You won't find any setup guides here, as it is too early - this repo is mostly to get my brain going, and try to think of alternative ways of create, manipulate and store music data. Later I will try to make a setup guide of sorts.

This project is a long-running quest to gain some control over my computer audio. I'm not particularly fond of coding in general - and I'd rather not have it as a job - but when I discovered SuperCollider in 2010, it changed my perspective towards using computers as a dynamic instrument. Being a musician, I value immediate feedback, and SC could, to some degree, convince me that the computer wasn't a static calculator.

The limitations of sequencers became more and more apparent while exploring sound stuff in SC. Sequencers seemed like just a collection of playable state, with much of the rigidity from analog studios and little of the charm. I don't think we need "virtual consoles" with 100+ faders which looks like old analog faders, for example. The console was the best you can do in the analog days, and there is no reason to stay there - yet it is the heart of every sequencer, and you use the mouse or a "dummy console" - a controller - to change its state.

While SuperCollider is great, it has a major - and dealbreaking flaw: Most of the great sounding code out there comes in the form of plugins, and plugin instruments, and SC has no vst/au/aax plugin support. 

While you don't see much daw innovation, there is a lot of plugin innovation (apart from the retro GUI stuff, slate and the like..), and some of the plugins outgrow their hosts, because they are getting increasingly more complex. Melodyne certainly did, and shipped their product with a standalone editor for convenience, and projects like Spat Revolution should have deeper access to the host. Such plugins need a far better system in the first place, as they require to see more state, and the plugin / host model might not be the best in the long term.

I can't say what the next DAW should look or feel like, but I think the complexities of music and sound deserves a more complex tool. Scripting audio dynamically should really be a mandatory part of a DAW. A normal-sized project can have thousands of audio clips, and lots and lots of data and metadata. A soundtrack for a film is way larger. There is a whole lot of transformations one could do, if one were given access.

### A database at the heart

The database is not part of this project, but I feel it should be. Here are some thoughts:

Every project in every sequencer ever is a collection of state - state that is manipulated and played from that particular sequencer (and no other - the omf format tries to deal with that). This state belongs in a queryable, open database that a host program can interact with. It might also be the case that you won't need the concept of a project, as the database should be able to contain every audio-related datum in your workspace. 

In a database, a project would just be an identifier, and everything that is connected to this identifier, directly or indirectly, is the project.

with such a database at the center, any musical idea can be added, and subsequently used/referenced, no matter what you're working on at the moment. A piece of datum could be a couple of midi events coupled to a synth of sorts - it can stay unconnected to anything, until you either expand on this idea (connecting more datums to it), or you already have a "project", where you link this idea in by connecting it to the project. This blurs the distinction between an idea, a small project, a big project, or a collection of projects, which is a good thing, since a larger project is a collection of smaller ideas, connected in a certain way.

Another issue is naming - when you create music, you often just play something, and at a certain point you hit record for whatever reason - at that point, you shouldn't worry about a precise name - a timestamp will often do - and if this is recorded directly to a database, you can later query the database for items recorded the lately. Chances are, you will find what you are looking for based on its content, and the content leaves a queryable data trail - midi events, recording length, which input was used, when - so it should be easy to retrieve something you've made without having 100's of "untitled" projects that you have to open and close, because the project in every daw is usually a closed, non-interactable container.

If you are coding music patterns, even this could be stored in a database in the same way. Audio graphs - connections between different processing units (eq, compressor, volume, sends) can be stored also. Every connection. Audio clips. Midi data. When you need it, you can "select" a subset of the database, and "play" it - and there is your sequencer, just a bit more liberated.

### An alternative, a queue

Another way to create a responsive system might be in the form of a queue. I still feel a lisp-type of lanugage would be great - i.ex. Clojure - it lets me express ideas more clearly, and at the same time it shields me from low-level details that I usually don't want to see. Clojure would be in another process, so communication would happen over an interprocess link of sorts.

DAWs are for the most part deterministic - the only part where max responsivity is needed is when the user interacts with the system - midi input, for example. This live midi input could be activated in C++ from Clojure, in that case C++ has to send messages back to Clojure with the received midi data (as it is Clojure's job to record)

For a message queue system to work one would have to build an API covering all needs. As a minimum, if you can play it in Pro Tools / Cubase / Reaper (minus the proprietary time stretching / variaudio / beat detective stuff), you should be able to play it here. Use cases would have to be tested against this API to see if this is the case. Preferably before anything is built.

Clojure should keep track of what is created, and take responsibility for freeing resources, and saving all state. Every entity in C++ would have an ID. C++ maintains the queue, where the most important queue is the audio callback queue, where the graph gets constructed every callback (clarify!). C++ doesn't need to know anything about the files and its length, one would utilize Clojure for file info. Time should be specified as seconds on the clojure side and converted to samples on the c++ side (nope, that puts unnecessary logic in c++). Apart from plugin editors, GUI can be drawn in clojure/java, messages from the GUI sent immediately to queue, and state stored within clojure. Even waveforms of files can be drawn, as C++ only knows the file handle anyway.

C++ maintains a queue for each plugin, clip, volume, and pan, in addition to the graph.

### A possible message API

I've looked at different messaging formats, and found that YAML might be the way to go, so YAML is used to describe the formatting of messages to/from the audio system. Any other message format can be used, as long as it supports binary.

All messages has a msgid field, which is generated by the caller, and copied to the response message of the callee. msgid could be an <integer>.
All message responses have an error field, which is a readable <string> intended for the user.

*integer* = 64-bit integer (preferably)
*real* = 64-bit double
*midi* = 7-bit unsigned integer 
*string* = a utf8-encoded string.
*path* = a *string* representing a path. Path delimiter: '/'. Windows drive specifiers allowed.

```YAML
# Returns the system samplerate
msg: samplerate
# return
samplerate: *integer*

msg: get-input-channels
# return
id: *integer*

msg: get-output-channels
# return
id: *integer*

msg: get-midi-inputs
# return
inputs: [ {name: <string>, id: <integer>}+ ]

# Returns an id if successful.
msg: load-plugin
path: *path*
# return
id: *integer* or null

# Returns a volume
# volume is parameterized
msg: create-volume
volume: *real* or null # defaults to 1
# return
id: *integer* or null

# Returns a panner
# pan is parameterized
msg: create-pan
pan: *real or null # defaults to 0 (center)
pan-law: "0db", "-3db", "-4.5db", "-6db" or null # defaults to -4.5 db
# return
id: *integer* or null

# Queues a sound clip
# Is a transient object, but id is returned for manipulation of playrate
msg: queue-clip
file: *integer*
dest-id: *integer*
dest-ch: *integer*
at-sample: *integer* or null # defaults to immediately
from-sample: *integer* or null # defaults to 0
end-sample: *integer* or null # defaults to last sample
playrate: *real* or null # defaults to 1
# return
id: *integer*

# Remove a plugin. Also removes the queue and updates the graph
msg: remove-plugin
id: *integer*

# Retrieves the state of a plugin
msg: plugin-get-state
id: *integer*
# return
state: *binary* or null

# Sets the state of a plugin
msg: plugin-set-state
id: *integer*
state: *binary*

# Opens plugin editor
msg: plugin-open-editor
id: *integer*

# Closes plugin editor
msg: plugin-close-editor
id: *integer*

# Queue a sequence of midi messages. If sample < system sample, the midi message is delivered immediately
msg: plugin-queue-midi
id: *integer*
midi: [ { sample: <integer>, bytes: [<midi-1>..<midi-N>] }+ ]

# Clears a midi queue from sample N. If sample is 0, clear the whole queue.
msg: plugin-clear-queue-midi
id: *integer*
sample: *integer*

# Queue a sequence of parameters. If sample < system sample, the parameter is delivered immediately
# Only VST3 has sample-accurate parameters, other formats gets updated for each buffer
msg: plugin-queue-param
id: *integer*
params: [ { sample: <integer>, param: <integer>, value: <real> }+ ]

# Clears a parameter queue from sample N. If sample is null, clear the whole queue.
msg: plugin-clear-queue-midi
id: *integer*
sample: *integer* or null

# Record a node to storage in 32 bit wav format. If sample-start is null, start recording immediately. If sample-end is null, the recording will continue until manually stopped. No recording if sample-start > sample-end.
msg: record-node
id: *integer*
path: <path>
sample-start: *integer* or null
sample-end: *integer* or null
# return
id: *integer*

# Record midi data from input node.
msg: record-midi
id: *integer*
sample-start: *integer* or null
sample-end: *integer* or null
# return
mididata: [ <byte>+ ]

# Stop recording node, even if sample-end is specified.
msg: stop-recording-node
id: *integer*

# Stop all recording
msg: stop-recording

# Load the wave file to memory. We might avoid the whole streaming scheme, by assuming that a user has a large amount of RAM. Streaming audio files from disks adds a complexity layer, but may be needed in the future. It depends on the use cases.
msg: load-wav-to-memory
path: *path*
# return
id: *integer* or null

# Remove file from memory
msg: remove-wav-file
id: *integer*

# Makes a connection between a source channel to a destination channel
# This should (I think) cause the source to be processed in the callback.
# The system will prevent you from making cyclic graphs.
# Delayed feedback loops can be added at a later stage
# We should be able to limit the duration of the connection, even though most connections are stable. Even if the backend can make optimizations to save cpu by not processing nodes that does not play, we can be explicit here.
# This also accepts input midi->plugin
msg: add-connection
source-id: *integer*
source-ch: *integer*
dest-id: *integer*
dest-id: *integer*
(start-sample: *integer* or null) # defaults to 0 - create connection immediately
(end-sample: *integer* or null) # defaults to 2**63 - keep connection forever

# removes a connection
# if source has no more connections, don't process the source in the callback
msg: remove-connection
source-id: *integer*
dest-id: *integer*
source-ch: *integer*
dest-id: *integer*

# clears all connections
msg: clear-connections

# remove anything with an id
msg: remove
id: *integer*


```

This message API depends on the programmer sending the right message to the right object at the right time. If a programmer sends a message to an object that doesn't exists, isn't playing, or an object of the wrong type, the message is simply ignored and not reported as an error.  

### Use cases

##### Monitor a microphone input
We got a microphone signal at input x that we want to monitor on the stereo speakers (y, z):
```YAML
{msg: get-input-channels} => input-id
{msg: get-output-channels} => output-id
{msg: add-connection, source-id: input-id, source-ch: 0, dest-id: output-id, dest-ch: 0}
{msg: add-connection, source-id: input-id, source-ch: 0, dest-id: output-id, dest-ch: 1}
```
##### Add Altiverb reverb send
Create a send for the input, at -12 db and and panned a bit to the right, open editor for plugin adjustments
```YAML
{msg: load-plugin, path: "c:/path/to/Altiverb 7.dll"} => altiverb-id
{msg: create-volume, volume: 0.25} => send-volume-altiverb-id
{msg: create-pan, pan: 0.4} => send-pan-altiverb-id
{msg: add-connection, source-id: input-id, source-ch: 0, dest-id: send-volume-altiverb-id, dest-ch: 0}
{msg: add-connection, source-id: send-volume-altiverb-id, source-ch: 0, dest-id: send-pan-altiverb-id, dest-ch: 0}
{msg: add-connection, source-id: send-pan-altiverb-id, source-ch: 0, dest-id: altiverb-id, dest-ch: 0}
{msg: add-connection, source-id: send-pan-altiverb-id, source-ch: 1, dest-id: altiverb-id, dest-ch: 1}
{msg: add-connection, source-id: altiverb-id, source-ch: 0, dest-id: output-id, dest-ch: 0}
{msg: add-connection, source-id: altiverb-id, source-ch: 1, dest-id: output-id, dest-ch: 1}
{msg: plugin-open-editor, id: altiverb-id}
```
