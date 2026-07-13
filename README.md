Pipeline
Here I create the Vulkan project to record my learning progress and also take some notes.

The rough pipeline is as below:
<img width="623" height="891" alt="Screenshot 2026-07-02 at 01 02 53" src="https://github.com/user-attachments/assets/f11a1278-54c7-4bd6-beaa-b94d057e6d7c" />

Output
The interesting part about output is that only three vertices are defined as red/blue/green, 
yet Vulkan has a magic bit - the rasterizer automatically interpolates values between vertices.
<img width="1592" height="1244" alt="image" src="https://github.com/user-attachments/assets/acc7d79b-7f34-4ac1-afb3-4ed923df9738" />
