# E-comOS Kernel - A Microkernel of E-comOS Operating System Project

This is the **core kernel** of E-comOS, it's a **microkernel**<br>

## License
License: AGPLv3 <br> 
Key rule (no need to read the whole license): If you modify this kernel or make derivative works (like your own OS based on it), you MUST open source your code under AGPLv3 (or a later version).  
For more details: Check the [Copyright Page in our Wiki](https://github.com/E-comOS-Operation-System/kernel/wiki).<br> 

## How to Contribute
### Submit an Issue Report
If you found out a bug, you can submit one bug report to our bug [tracker](https://github.com/e-comos/kernel/issue), when you submit the report, you write these in your report:
 - What were you doing when the problem occurred?
 - What did you expect to happen?
 - What actually happened?
 - What operating system are you using?
 - What CPU architecture are you using?
 - What version of the kernel are you using?
 - What version of the compiler are you using?
 - Or , can you submit the screenshot with your report?
We'll fix your problem quickly, but some problems we won't fix, because may be it's yourself or your computer's problem.<br>
If we encounter this situation, we will mark your issue report as "WONTFIX," meaning we will not fix the issue.<br>
### Translate
There are some comments in codes, some documents or others using English only , or other language only.<br>
We need some people to translate them, if you're going to translate, please `git clone` our repo first, and send a patch to our mailing list <e-comos-kernel@groups.io>.
### Codes
The codes is most important, if you can, please join up us to develop E-comOS, we're using **1TBS, 4-bits tab and snake_case** **only**.
We'll **reject** use the **Allman, GNU style** and **spaces, camlCase** codes in our codes.<br>
E-comOS unsupport the pull request via GitHub, please use the mailing list.
#### Vibe Code
Coder can use the AI to help they. But, you've to know, if you want to use the AI(vibe code), you **MUST**:
 - Review your code by yourself;
 - **CAN'T** send the patch and using AI to provide responses during code reviews;
 - You **CAN'T** use the AI to generate the ideas; and
 - The copyright is **YOURS**, when you push the code, your copyright line should like this:
```
        Copyright(C) YOUR NAME   YEARS
```
Not the AI compnay's name.

---
About more infomations, see [E-comOS Coding Style Guide](https://github.com/e-comos/e-comos/blob/main/docs/coding-style.md) to learn more.
## How to Make a Runable Image
First , please :
1. Clone the repository: `git clone https://github.com/e-comos/kernel.git`
2. Run `./build.pl clean` to make this repo is clear
3. Run `./build.pl` to get `kernel.elf`.
4. Test in QEMU , please `mkdir -p iso/boot/` and `grub2-mkrecues -o <output file name> iso/`
5. At last, run this output file in QEMU.