### Project 2: User Programs

#### Group Participants
Michael Hedrick  
Matthew Moltzau  

#### Working Directory and Pintos Utilities
We will be working primarily from `src/userprog`, which is where the kernel is
located. You can invoke "make" just like normal.
EDIT: Change to be vm instead of userprog

For testing, run `pintos-p2-tests` (kudos to Brian). The bash script is located
in `src/userprog`, but an alias exists in `src/utils` so that you can call it
from anywhere.
EDIT: pintos-p3-tests, kudos to John

A bash script `pintos-run-util` was just created that helps with loading and
running user programs. It can run tests, but is limited since it can't pass
the correct arguments to right tests just yet.
EDIT: Not tested for src/vm yet.

The grader will be using `make check` from the build directory.

TODO update
#### Pintos Project 2 Assignments
Argument Passing  
Handling User Memory   
System Call Infrastructure  
File Operations (using fd)  

#### Design Considerations
See `DOC_p3.md` for the design document.

See the folder "pintos guides" for the IA-32 manuals and three different pdfs on pintos. One is from Ivo, one was given to us initially, and the other just seemed very helpful. We should probably reference these in the design doc once the project is complete.

