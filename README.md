open the working directory like in my case
~/desktop/quiver
then run make build-debug
then cd ./build/debug/
then ./quiver <image name>:version (in my case python:latest)
then output will have containers attach socket in my case /home/anish/.quiver/containers/<pid>/attach.sock 
then ./quiver attach <path to container socket>  (in my case ./quiver attach /home/anish/.quiver/containers/<pid>/attach.sock)(to go in attach mode)
then u will be attached to the container 
to go in detach mode press ctrl+p and then ctrl+q
then u can again go to attach mode using the commands metioned above
then u type exit in the container then it will be automatically closed and removed.
