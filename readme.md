The OpenLoop is an experimental project, where the Juce Framework is exposed through ~~Lua~~ Chaiscript bindings.

Originally I wanted to use a lisp-type of language, but Chaiscript was the easiest to bind - and the most important thing for now is to get some sort of dynamism up and running.

This project is a long-running quest to gain some control over my computer audio. I'm not particularly fond of coding in general - and I'd rather not have it as a job - but when I discovered SuperCollider in 2010, it changed my perspective towards using computers as a dynamic instrument. Being a musician, I value immediate feedback, and SC could, to some degree, convince me that the computer wasn't a static calculator.

The limitations of sequencers became more and more apparent while exploring sound stuff in SC. Sequencers seemed like just a collection of playable state, with much of the rigidity from analog studios and little of the charm. I don't think we need "virtual consoles" with 100+ faders which looks like old analog faders, for example. And you can't easily opt out of this model, either - if you're in Cubase or Pro Tools, you're stuck - these companies don't innovate anymore.

While SuperCollider is great, it has a major - and dealbreaking flaw: Most of the great SOUNDING code out there comes in the form of plugins, and plugin instruments, and SC has no plugin support. While you don't see much daw innovation, there is a lot of plugin innovation (apart from the retro GUI stuff, slate and the like..), and some of the plugins outgrow their DAWs very soon. Melodyne certainly did, and projects like Spat Revolution will. Such plugins need a far better host.

I can't say what the next DAW should look or feel like, but I think the complexities of music and sound deserves a more complex tool. Scripting audio dynamically should really be a mandatory part of a DAW. A normal-sized project can have thousands of audio clips, and the state of every plugin, pan position, and volume - moving through time - can be staggering. A soundtrack for a film is way worse. There is a whole lot of transformations one could do, if one were given access.

TODO: write more!
