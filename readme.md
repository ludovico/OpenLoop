### The OpenLoop project

The OpenLoop is an experimental project, where the Juce Framework is exposed through ~~Lua~~ Chaiscript bindings. 

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

Another way to create a responsive system might be in the form of a queue. I still feel a lisp-type of lanugage - Clojure is great -it lets me express ideas more clearly, and at the same time it shields me from low-level details that I usually don't want to see. Clojure would be in another process, so communication would happen over an interprocess link of sorts.

DAWs are for the most part deterministic - even the random bits are - the only part where max responsivity is needed is when the user interacts with the system - midi input, for example. The inputs could still be activated from another process, so one could have as good responsitivity as possible.

For a message queue system to work one would have to build an API covering all needs. Trying to build a list:
- open/close a file handle
- open/close a vst (and its editor)
- send vst state
- specify a graph (including inputs and outputs) at any point in time (present -> future)
- specify a graph for offline processing
- as little logic in c++ as possible
- assign midi and parameter messages to any node at any time (present -> future)
- c++ should have a decent sample player and volume processor node
- clear queue

C++ could respond:
- signal value of given nodes
- advancement in samples for each callback call
- send vst state
- send midi and parameter input data (with timestamp) for clojure to record.

Clojure should keep track of what is created, and take responsibility for freeing resources, and saving all state. Every entity in C++ would have an ID. C++ maintains the queue, where the most important queue is the audio callback queue, where the graph gets constructed every callback. C++ doesn't need to know anything about the files and its length, one wpuld utilize Clojure for file info. Time should be specified as seconds on the clojure side and converted to samples on the c++ side. Apart from plugin editors, GUI can be drawn in clojure/java, messages from the GUI sent immediately to queue, and state stored within clojure. Even waveforms can be drawn, as C++ only knows the file handle anyway.

So is it is possible, then, to manipulate all state, using another language and its logic, and let c++ be a performant, but dumb receiver of messages?
