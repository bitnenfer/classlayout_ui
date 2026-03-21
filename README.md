ClassLayout UI
---

ClassLayoutUI lets you inspect a PDB file and analyze how memory is layed out and how much is wasted to padding.
Sometimes when you're scrapping for the last bytes this could be helpful. It could also be useful to see if data is layed out in a cache friendly way.
This is an executable version of my web tool [ClassLayout](https://bitnenfer.com/classlayout/). Since the browser has memory restrictions, I wanted to have a tool that could open large PDB files.

![](https://bitnenfer.com/nop/classlayout_ui.png)