# rowhammer

Memory tester for RowHammer. (Built on top of Memtest86+ v5.01.)

RowHammer is a new type of **memory failure** that is found only in recent generations of DRAM chips \[1\]. Just by reading from the same address >100K times, it is possible to corrupt data in nearby addresses. More technically, repeated "activations" to the same DRAM row corrupts data in adjacent DRAM rows. 

RowHammer is a **security threat** since it allows a malicious program to breach memory protection. This is because different DRAM rows are mapped to different software pages. As of mid-2014, the problem is not well publicized.

[\[1\] Kim et al. *Flipping Bits in Memory Without Accessing Them.* ISCA 2014.](http://users.ece.cmu.edu/~yoonguk/papers/kim-isca14.pdf)

### Getting Started

There are two ways of starting the test: from a bootloader or from a disk.

- Option 1: From bootloader (e.g., GRUB2)  

        $ cd rowhammer/src
        $ make
        $ sudo cp memtest.bin /boot 
        $ sudo vi /boot/grub2/grub.cfg    # add new entry
        $ sudo reboot
        # Select new entry from GRUB2 menu

- Option 2: From disk

        $ cd rowhammer/src 
        $ make iso
        $ sudo dd if=mt501.iso of=/dev/...
        $ sudo reboot

### Running the Test

After the Memtest86+ screen appears, the RowHammer test will start to run automatically after several seconds of delay. The test will continue to run until the progress bar on the upper-right corner of the screen reaches 100%. At this time (or any other time), you can press 'ESC' to reboot the system.

At a high level, the test consists of six steps. (For more details, please refer to Section 4 of our paper [1].)

1. Populate entire DRAM with all '0's.
2. Repeatedly read from a pair of DRAM rows in an interleaved manner.
    - This ensures that both DRAM rows are repeatedly activated.
3. Repeat Step 2 for different pairs of DRAM rows.
4. Check DRAM for errors (i.e., '1's).
5. Print errors on screen in bright red.
6. Repeat Steps 1-5 for all '1's.

### Caveats

The RowHammer test is *not* comprehensive. Passing the test does *not* guarantee that your DRAM is free from errors. This is because of three reasons.

- *Reason 1: Only a few rows are tested.* Due to the sheer number of reads to each row, exhaustively testing every row could take up to several days. Instead, we test only a few rows so that the test "completes" within a reasonable amount of time. (Nevertheless, on an Intel Haswell, the test induced 17 errors in 1.5 minutes for an exceptionally problematic DRAM module.) However, it is trivial to increase the number of tested rows by editing the source code.

- *Reason 2: There may be better access-patterns.* The more quickly a row is activated, the more errors it induces in its adjacent rows. However, our particular way of accessing DRAM (interleaved reads to two rows, cache-line flush, and memory fence) imposes a large delay between activations to the same row. Quicker ways of activating a row may exist.

- *Reason 3: There are better data-patterns.* We test DRAM with two different data-patterns: all '0's and all '1's. However, these data-patterns are actually the worst for inducing errors. To our knowledge, one of the best data-patterns is alternating rows of '0's and '1's. Unfortunately, achieving this particular data-pattern ("RowStripe") is difficult because of two complications.

    - *Data-scrambling by memory controller.* For electrical reasons, it is desirable to send/receive an even mix of '0's and '1's over the memory bus. To increase the chance of doing so, the memory controller scrambles the data it sends to DRAM. This creates a mismatch between the data-pattern that the CPU thinks it is writing and the data-pattern that is actually being written in DRAM.
 
    - *Address-swizzling by memory controller.* To extract memory parallelism, the memory controller "swizzles" the physical address (by utilizing a combination of bit-wise permutations and XORs) before it accesses DRAM. This makes it difficult to determine which physical addresses correspond to which DRAM rows.

By overcoming all of the above, it is possible to increase the number of errors by many orders of magnitude. As an example, we were able to increase the number of errors from 17 to 10,000,000 for one exceptionally problematic DRAM module. The latter number was obtained using custom-built hardware equipment that allows us to test every row in the DRAM module (within a relatively short amount of time)  using the best access-pattern and the best data-pattern. (For more details, please refer to Section 5 of our paper [1].)

### Contributors

- Yoongu Kim (Carnegie Mellon University)
