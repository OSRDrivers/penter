# About penter #

Many years ago, we were working on a file system filter driver that would absolutely **destroy** the performance of a system under certain workloads. Operations that previously only took a few minutes would turn into hours. We were also pretty convinced that there were some workloads that would  now literally take "forever", though we never had the patience to actually wait that long.

The project was incredibly complex and we were having difficulty narrowing down the exact reason for the degradation. CPU utilization was low. Disk utilization was low. We even started removing locks to see if being wrong could make us faster (it didn't). 

After scratching our heads for a bit, we decided that it might be useful to see the aggregate elapsed times of all the functions in our filter suite. Given that we had such a disparity in elapsed time when our filter was involved, we assumed that we should be able to get better insight into what was going on if we could find "fat" functions.

Thus, the penter tracing library was born. If you compile your code using the VisualC compiler's /Gh and /GH switches, calls to the _penter and _pexit functions will be embedded at the start and end of each function in your module. By writing our own _penter and _pexit hooks, we kept track of the amount of time spent in each function of the driver. We then created a WinDbg debugger extension to extract the data into a CSV format that we could easily import into Excel.

We didn't build it to be pretty. We didn't build it to be fast. BUT, it did absolutely help us narrow down what the problems were (they were flushes!). We also had quite a difficult time getting the penter hooks to work on x64. So, we thought we'd share the code in case it's useful for someone else (either to track down a problem or as an example of a penter hook on x64). 

# Building penter Library and Debugger Extension #
The provided solution builds the penterlib static library containing the _penter and _pexit hooks. The code supports Debug and Release builds on both the x86 and x64.

It also builds the penterkd.dll WinDbg debugger extension. This extension will be used to collect the trace data from the target system.

# Adding penter Tracing to a Project #
If you want to add penter support to a driver project add the /Gh and /GH compiler options. Once you do so you will receive errors about _penter and _pexit not being defined for your module. Adding the penterlib.lib file as a library dependency will then resolve the compilation errors.

The repo also provides a penter.props file that you can include in your vcxproj to automatically set the appropriate compile and link flags. 

# Extracting Trace Information #
Once your driver is compiled with the necessary hooks, load the penterkd Debugger Extension on your host machine:

    0: kd> !load E:\repos\Developer\SNoone\penter\x64\Debug\penterkd.dll
    
Now you can dump out the trace data to collect statistics on the module:

    0: kd> !modulestats scanner
    Function,CallCount,CallTicks,TicksPerCall
    scanner!DriverEntry,1,4715,4715
    scanner!ExInitializeDriverRuntime,1,37,37
    scanner!ScannerInitializeScannedExtensions,1,333,333
    scanner!KeGetCurrentIrql,4298,621,0
    scanner!ScannerAllocateUnicodeString,6,113,18
    scanner!ScannerInstanceSetup,9,173,19
    scanner!ScannerPreCleanup,3389,2843,0
    scanner!ScannerPreCreate,4281,98602,23
    scanner!ScannerPostCreate,4281,4189123,978
    scanner!ScannerPreFileSystemControl,969,1318,1
    scanner!ScannerpCheckExtension,3422,6356,1
    scanner!ScannerPreWrite,186,143,0
    scanner!ScannerpScanFileInUserMode,203,3485008,17167
    scanner!ScannerPortConnect,1,22,22

The data can now be easily imported into Excel. Generally the column of most interest is TicksPerCall, which is the total number of ticks used by the function divided by the number of calls to the function. The bigger the number the more time you're spending in that function.
