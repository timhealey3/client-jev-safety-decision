## TCP and JEV learning
This is a small toy project created to help me understand how TCP sockets work.
I also wanted to test out the JEV API

# Project Idea
An L4 autonomous vehicle agent sends data to a server about the environmnet.
The server responds with commands about what the vehicle agent should do, such as
pullover, require driver attention, require driver intervention, or continue operating
autonomously.

# Architecture
A client will send information about its environment. In this case it will pick out a string at random and send it out.
But the idea would be in the future this could be integrated in with various sensors and cameras,
to generate real environment read outs.

The server gets the environment strings, and sends it to a OpenRouter Jev API.
