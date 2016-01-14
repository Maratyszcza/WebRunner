n = Argument(uint64_t, "iterations")

with Function("playme",
    # Function arguments (here we have just one argument, "iterations")
    (n,),
    # Target microarchitecture and ISA
    target=uarch.default + isa.sse3):

    # Load our only argument into 64-bit register.
    # This is normally a no-op: PeachPy would use the same
    # register where the argument was passed to the function
    reg_n = GeneralPurposeRegister64()
    LOAD.ARGUMENT(reg_n, n)

    # This is an assembly loop. We use it to make processor re-run the same block of instructions.
    with Loop() as loop:
        # This is a Python loop. We can use it to generate assembly code. 
        for i in range(0, 16, 2):
             xmm_i, xmm_j = XMMRegister(i), XMMRegister(i+1)
             ADDSUBPS(xmm_i, xmm_i)
             MULPS(xmm_j, xmm_j)

        DEC(reg_n)
        JNZ(loop.begin)

    # No need to preserve/restore registers: PeachPy would handle that for us.
    RETURN()

