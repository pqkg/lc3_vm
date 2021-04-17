# lc3_vm
Simple project I used to learn more about computer architecture

* Based on/resource I used: https://justinmeiners.github.io/lc3-vm/
* ISA: https://justinmeiners.github.io/lc3-vm/supplies/lc3-isa.pdf
* Slightly modified (purpose of this project/article read was mainly to learn a bit more about low-level details, not to write a VM), but I still think it can't hurt to upload it (if I ever wanna look back at the resources I used to learn, for example)
* Intended as the final project for Harvard's online course CS50x 
* Most code, besides the platform specific one (next bullet), I wrote by myself after reading the theory explained in the article. After that, I looked at the code written by the article author and improved mine (wouldn't learn if I'd just write programs without looking at the code written by more experienced people, ex. I didn't know about enums before)
* I copied all the platform specific code from the linked website (as said have mostly done this project/read the mentioned article to learn more low level details, not to write a fantastic VM or to learn about tedious platform specific code), also I'm still a noob

* Few questions/confusions I have (just documenting once again):
* Why not use signed numbers instead of uint, as then C would take care of things like sign extending (I assume)? Perhaps for simplicity?
* Why is LC33 not byte addressable?
* Why does specification say to add offsets to the already incremented PC, instead of current one?

* Finally, a huge thanks to David Malan, Brian Yu, the rest of the CS50 staff, Justin Meiners and anyone else who contributed to CS50 or the linked article
 
