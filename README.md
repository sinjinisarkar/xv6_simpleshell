# xv6_simpleshell
A custom shell implemented for the xv6 operating system as part of a university coursework project. This shell includes core features like command execution, input/output redirection, multi-element pipelines, and sequential command execution, designed to demonstrate a deep understanding of operating system principles.

# Features
* Command Execution: Execute simple and complex commands with support for whitespace variations.
* Input/Output Redirection: Handle input (<) and output (>) redirection seamlessly.
* Pipelines: Support two-element and multi-element pipelines (e.g., ls | grep test | cat).
* Sequential Commands: Execute multiple commands in sequence using the ; operator.
* Edge Case Handling: Robust against varied command formats, such as extra spaces or unusual input patterns.

# Setup
### Prerequisites  
* xv6 operating system source code  
* GCC or a compatible C compiler  
* Git  

### Installation  
1. Clone the xv6 repository:  
   ```bash  
   git clone https://github.com/mit-pdos/xv6-public.git  
   cd xv6-public
   ```
2. Clone this project into the user/ directory of xv6: 
   ```bash
   git clone https://github.com/<your-github-username>/xv6_simpleshell.git user/  
   ```
3. Build the xv6 OS:
   ```bash
   make clean  
   make qemu  
   ```
