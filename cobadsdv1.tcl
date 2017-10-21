#----------------------------------------------------------------
# Definition of the physical layer 
#----------------------------------------------------------------
set val(chan)       Channel/WirelessChannel 
set val(prop)       Propagation/TwoRayGround 
set val(netif)      Phy/WirelessPhy 
set val(mac)        Mac/802_11 
set val(ifq)        Queue/DropTail/PriQueue 
set val(ll)         LL 
set val(ant)        Antenna/OmniAntenna 
#-----------------------------------------------------------------
# Scenario parameters
#------------------------------------------------------------------
set val(x)              2000   ;# X dimension of the topography 
set val(y)              2000   ;# Y dimension of the topography 
set val(ifqlen)         100    ;# max packet in queue
set val(seed)           0.0    ;#random seed            
set val(adhocRouting)   DSDV   ;
set val(nn)             50  ;# how many nodes are simulated
set val(cp)             "traffic2"; 
set val(sc)             "skenario.tcl";
set val(stop)           200        ;# simulation time


#---------------------------------------------------------------------
#  Set up simulator objects                  
#---------------------------------------------------------------------
# create simulator instance
set ns_     [new Simulator]
# setup topography object
set topo    [new Topography]
# create trace object for ns and nam
set tracefd [open out.tr w]
$ns_ use-newtrace  ;# use the new wireless trace file format
set namtrace    [open out.nam w]

$ns_ trace-all $tracefd
$ns_ namtrace-all-wireless $namtrace $val(x) $val(y)
# define topology
$topo load_flatgrid $val(x) $val(y)
# Create God
set god_ [create-god $val(nn)]
$ns_ node-config -adhocRouting $val(adhocRouting) \
             -llType $val(ll) \
             -macType $val(mac) \
             -ifqType $val(ifq) \
             -ifqLen $val(ifqlen) \
             -antType $val(ant) \
             -propType $val(prop) \
             -phyType $val(netif) \
             -channelType $val(chan) \
             -topoInstance $topo \
             -agentTrace ON \
             -routerTrace ON \
             -macTrace ON


#  Create the specified number of nodes [$val(nn)] and "attach" them
#  to the channel.

for {set i 0} {$i < $val(nn) } {incr i} {
    set node_($i) [$ns_ node]

    #$ns at 0.0 "$node_(0) color blue"  
    $node_($i) random-motion 0      ;# disable random motion
}
for {set i 0} {$i < $val(nn) } {incr i} {
    $node_($i) color blue
    $ns_ at 0.0 "$node_($i) color cyan"
}
# Define node movement model
puts "Loading connection pattern..."
source $val(cp)
# Define traffic model
puts "Loading scenario file..."
source $val(sc)
# Define node initial position in nam
for {set i 0} {$i < $val(nn)} {incr i} {
# 50 defines the node size in nam, must adjust it according to your scenario
# The function must be called after mobility model is defined
# puts "Processing node $i"
$ns_ initial_node_pos $node_($i) 50
}

#
# Tell nodes when the simulation ends
#
for {set i 0} {$i < $val(nn) } {incr i} {
    $ns_ at $val(stop).0 "$node_($i) reset";
}

$ns_ at  $val(stop).0002 "puts \"NS EXITING...\" ; $ns_ halt"

# dump the initial simulation info to the trace file

puts $tracefd "M 0.0 nn $val(nn) x $val(x) y $val(y) rp $val(adhocRouting)"
puts $tracefd "M 0.0 sc $val(sc) cp $val(cp) seed $val(seed)"
puts $tracefd "M 0.0 prop $val(prop) ant $val(ant)"

puts "Starting Simulation..."
$ns_ run
