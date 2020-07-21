---
layout: post
title: "DIY motorized blinds for $40"
date: 2016-05-20 10:00:00
---

I have some 2" wooden blinds in my house that I've been wanting to motorize. Why? I'm lazy and I thought it would be cool to have.

The best commercial solution for retrofitting existing blinds seems to be [Somfy](https://www.somfysystems.com/). They have wireless battery-powered systems and fancy-looking remotes. For new motorized blinds, [Bali](http://www.baliblinds.com/) seems to be popular, and they use Somfy for the motorization. There are also some kickstarter things ([MOVE](http://www.teptron.com/), [MySmartBlinds](http://www.mysmartblinds.com/)), but the last time I looked those didn't really do what I want. Somfy likely has a good product, but it's very expensive. It looks like it would cost about $150 per blind, which is just way too much for me. They want [$30 just for the plastic wand](http://www.automatedshadestore.com/shop/product-info.php?Somfy_12V_DC-Volt_Reloadable_Battery_Tube_Wand-pid328.html) that holds the batteries (8 x AA). We're talking about a motor and a wireless controller to tell it what to do. It's not rocket surgery, so why should it cost $150?

My requirements are:

* Ability to tilt the blinds to one of three positions (up, middle, down) remotely via some wireless interface. I don't care about raising or lowering the entire blind.
* There must be some API for the wireless interface such that I can automate them myself (close at night, open in morning)
* Tilt multiple blinds at the same time so they look coordinated.
* Be power efficient -- one set of batteries should last more than a year.

Somfy satisfies this if I also buy their ["Universal RTS Interface"](http://www.amazon.com/Somfy-Universal-Interface-Channel-1810872/dp/B00OD3CCUQ%3FSubscriptionId%3DAKIAILSHYYTFIVPWUY6Q%26tag%3Dduckduckgo-ffab-20%26linkCode%3Dxm2%26camp%3D2025%26creative%3D165953%26creativeASIN%3DB00OD3CCUQ) for $233, but that only makes their solution even more expensive. For the 6 blinds I wanted to motorize, it would cost about $1200. No way.

I've been meaning to get into microcontrollers for a while now, and I thought this would be the perfect project for me to start. About a year ago I bought a [RedBear BLE Nano](http://redbearlab.com/blenano) to play with some Bluetooth stuff, so I started with that. I got a [hobby servo](https://www.sparkfun.com/products/11965) and a bunch of other junk (resistors, capacitors, etc) from [Sparkfun](https://sparkfun.com) and began flailing around while I had some time off around Christmas. The Arduino environment on the BLE Nano is a little weird, but I got things cobbled together relatively quickly. The servo was very noisy, and it's difficult to control the speed, but it worked. Because I wanted to control multiple devices at once, BLE was not a really great option (since AFAIK there is no way to 'broadcast' stuff in a way that is power-efficient for the listeners), and I started looking at other options. Eventually I ran across the [Moteino](https://lowpowerlab.com/shop/Moteino/moteino-r4).

The Moteino is an Arduino clone paired with a RFM69W wireless radio, operating at either 915Mhz or 433Mhz. It also has a very efficient voltage regulator, making it suitable for battery powered applications. The creator of the board (Felix Rusu) has put in a lot of work to create libraries for the Moteino to make it useful in exactly my type of application, so I gave it a try. The [RFM69 library](https://github.com/LowPowerLab/RFM69) is lovely to work with, and I was sending messages between my two Moteinos in no time. The idea is to have one Moteino connected via USB to a Linux box (I already have a [BeagleBone Black](http://beagleboard.org/black)) as a base station which will relay commands to the remote devices. I got my servo working again with the Moteino quickly, as most of the code Just Worked.

I started out with a hobby servo because I knew it would be easy to control, but the noise and lack of speed control really bothered me. I needed to try something else. I considered higher quality servos, a gear motor with encoder or limit switches, stepper motors, worm gear motors, etc. I was going to end up building 6 of these things to start with, so cost was definitely a big factor. I ended up settling on the 28BYJ-48 stepper motor because it is extremely cheap (about $2), relatively quiet, and let me control the speed of rotation very precisely. There is a great Arduino library for stepper motors, [AccelStepper](http://www.airspayce.com/mikem/arduino/AccelStepper/), which lets you configure acceleration/deceleration, maximum speed, etc. It also has an easy-to-use API for positioning the motor. I found a [5mm x 8mm aluminum motor coupling](http://www.ebay.com/itm/21Size-D19L25-Flexible-Shaft-Coupling-CNC-Stepper-Motor-Coupler-Connector-4-10mm-/381412205587?var=&hash=item58cdf06413:m:matJKF7ONfwQKgTUkCL-55A) to connect the motor to the blinds shaft. I then used a zip tie and a piece of rubber to secure the motor to the blinds rail. This doesn't look very professional, but it's not something you really see (my blinds have a valance that covers the rail). A better solution involving some kind of bracket would be great, but would increase the cost and require a lot more time. Using the stepper, I was able to smoothly, quietly, and cost-effectively control the blinds.

I then started to look into power consumption. If you don't do put anything to sleep, the power usage is pretty high. The [LowPower](https://github.com/LowPowerLab/LowPower) library from Felix makes it easy to put the CPU to sleep, which helps a lot. When sleeping, the CPU uses very little power (about 3µA I think), and the radio will wake you up via interrupt if a message arrives. The radio uses roughly 17mA in receive mode, however, so that means we'd only get about a week of battery life if we used a set of high-quality AAs (`3000mAh / 17mA = 176h`). We need to do a lot better.

The RFM69 has a useful feature called Listen Mode that some folks on the LowPowerLabs [forums](https://lowpowerlab.com/forum/index.php/board,7.0.html) have figured out how to use. In this mode, you can configure the radio to cycle between sleeping and receiving in order to reduce power consumption. There are a lot of options here, but it was discovered that you only need to be in the RX phase for 256µS in order for a message to be detected. When the radio is asleep it uses about 4µA. So if you sleep for 1s and receive for 256µS, that means your average power consumption for the radio is about 12µA. This is a dramatic improvement, and it means that the device can still respond in roughly one second, which is certainly adequate for my application. Of course, you can always trade even more responsiveness for power efficiency, and people using this method on coin cell batteries certainly do that. There is one user on the forums who has an application with an expected battery life of over 100 years on a single coin cell! I have an extension of the RFM69 library, [RFM69_WL](https://github.com/snorp/RFM69_WL), which collected some of the other listen mode code that was floating around and extends it so you can set your own sleep/RX durations.

I've measured/calculated my average power consumption to be about 46µA if I run the motor for 12s per day. That comes out to *over 7 years of life on a set of 4 AA batteries* (Energizer Ultimate Lithium), which is an almost unbelievable number. There are several factors I am not really considering, however, such as RF noise (which wakes the radio causing increased power consumption), so the real life performance might not be very close to this. Still, if I can get 2 years on a set of 4 AAs I'll be pretty happy.

Usually when you buy a 28BYJ-48 it will include a driver board that has a [ULN2003A](http://www.ti.com/product/uln2003a), some connectors, and a set of LEDs for showing which phase of the stepper is active. This is fine for testing and development, but it was going to be pretty clunky to use this in a final solution. It was time to design my first PCB!

I found out early on that it was pretty difficult to talk to other people about problems with your project without having a schematic, so I made one of those. I started with [Fritzing](http://fritzing.org/home/), but moved to [EAGLE](http://www.cadsoftusa.com/) when it was time to do the PCB. It seemed to be the standard thing to use, and it was free. EAGLE has a pretty steep learning curve, but some [tutorials](https://learn.sparkfun.com/tutorials/using-eagle-schematic) from Sparkfun helped a lot. I also got some help from the folks on the LowPowerLabs forums (TomWS, perky), who I suspect do this kind of thing for a living. You can get the EAGLE schematic and board design [here](https://github.com/snorp/BlindsControl/tree/master/PCB).

<blockquote class="imgur-embed-pub" lang="en" data-id="aK5LqnU"><a href="//imgur.com/aK5LqnU">View post on imgur.com</a></blockquote><script async src="//s.imgur.com/min/embed.js" charset="utf-8"></script>
<blockquote class="imgur-embed-pub" lang="en" data-id="bustGg8"><a href="//imgur.com/bustGg8">View post on imgur.com</a></blockquote><script async src="//s.imgur.com/min/embed.js" charset="utf-8"></script>

I ordered my first batch of boards from [Seeed Studio](http://seeedstudio.com/fusion_pcb.html), as well as a bunch of supporting components from [Digikey](http://www.digikey.com/). The PCB orders typically take a little over two weeks, which is quite a bit more waiting than I'm accustomed to. I was pretty excited when they arrived, and started checking things out. I soon realized I had made a mistake. The component I had in my design for the motor connector (which is a [JST-XH 5-pin](http://www.digikey.com/product-detail/en/jst-sales-america-inc/B5B-XH-A\(LF\)\(SN\)/455-2270-ND/1530483)) was the wrong pitch and size, so the socket I had didn't fit. Whoops. The hardware world does not play well with my "just try some stuff" mentality from working with software. I found an EAGLE library for the JST-XH connectors, used the correct part, and ordered another batch of PCBs. This time I actually printed out my board on paper to make sure everything matched up. I had run across [PCBShopper](http://pcbshopper.com/) while waiting for my first batch of boards, so I decided to use a different fabricator this time. I chose [Maker Studio](http://makerstudio.cc/) for the second order, since I could pay about the same amount and get red boards instead of green. Another two weeks went by, and finally last week I received the boards. I assembled one last weekend using my fancy (and cheap!) new [soldering station](http://www.amazon.com/Aoyue-Variable-Soldering-Station-Removable/dp/B00MCVCHJM?ie=UTF8&psc=1&redirect=true&ref_=oh_aui_detailpage_o00_s00). It didn't work! Shit! The Moteino was working fine, but the motor wasn't moving. Something with the motor driver or connection was hosed. After probing around for a pretty long time, I finally figured out that the socket was installed backwards. It seems the pins in the EAGLE part I found were reversed. Ugh. With a lot of hassle, I was able to unsolder the connector from the board and reverse it. The silkscreen outline doesn't match up, but whatever. It works now, which was a big relief.

<blockquote class="imgur-embed-pub" lang="en" data-id="4X1vFAO"><a href="//imgur.com/4X1vFAO">View post on imgur.com</a></blockquote><script async src="//s.imgur.com/min/embed.js" charset="utf-8"></script>

I thought about putting the board in some kind of plastic enclosure, but it was hard to find anything small enough to fit inside the rail while also being tall enough to accomodate the Moteino on headers. I'm planning to just use some extra-wide heat shrink to protect the whole thing instead, but haven't done that yet.

Below are some photos and videos, as well as the entire list of parts I've used and their prices. Each device costs about $40, which is a pretty big improvement over the commercial options (except maybe the kickstarter stuff). Also important is that there are no wires or electronics visible, which was critical for the Wife Acceptance Factor (and my own, honestly).

I'm sure a real EE will look at this and think "pfft, amateur!". And that's fine. My goals were to learn and have fun, and they were definitely accomplished. If I also produced something usable, that's a bonus.

<blockquote class="imgur-embed-pub" lang="en" data-id="6y5y8fg"><a href="//imgur.com/6y5y8fg">View post on imgur.com</a></blockquote><script async src="//s.imgur.com/min/embed.js" charset="utf-8"></script>

<blockquote class="imgur-embed-pub" lang="en" data-id="asrs8x3"><a href="//imgur.com/asrs8x3">View post on imgur.com</a></blockquote><script async src="//s.imgur.com/min/embed.js" charset="utf-8"></script>

<blockquote class="imgur-embed-pub" lang="en" data-id="LEehhZg"><a href="//imgur.com/LEehhZg">View post on imgur.com</a></blockquote><script async src="//s.imgur.com/min/embed.js" charset="utf-8"></script>

<div>
	<iframe width="315" height="315" src="https://www.youtube.com/embed/l3H9lPdafEg?list=PLbksjsxwAq-dJseSDi6oBmee6cvs_IarQ&index=1" frameborder="0" allowfullscreen></iframe>
</div>

### Bill of Materials

<table>
	<tr><td><a href="http://www.ebay.com/itm/Stepper-Motor-ULN2003-5-Line-4-Phase-5V-12V-28BYJ-48-Drive-Test-Module-Board-/291527657122?hash=item43e06706a2:g:3KAAAOSwHnFVuvJm">28BYJ-48-12V</a></td><td>$2.08</td></tr>
	<tr><td><a href="http://www.digikey.com/product-detail/en/on-shore-technology-inc/ED16DT/ED3046-5-ND/4147596">DIP socket</a></td><td>$0.19</td></tr>
	<tr><td><a href="http://www.ebay.com/itm/21Size-D19L25-Flexible-Shaft-Coupling-CNC-Stepper-Motor-Coupler-Connector-4-10mm-/381412205587?var=&hash=item58cdf06413:m:matJKF7ONfwQKgTUkCL-55A">Motor Coupling, 5mm x 8mm</a></td><td>$1.26</td></tr>
	<tr><td><a href="http://www.digikey.com/product-detail/en/rubycon/35ZLH100MEFC6.3X11/1189-1300-ND/3134256">Electrolytic Capacitor, 100µF</a></td><td>$0.30</td></tr>
	<tr><td><a href="http://www.digikey.com/product-detail/en/vishay-bc-components/K104K10X7RF5UH5/BC2665CT-ND/2356879">Ceramic Capacitor, 0.1µF (2)</a></td><td>$0.24</td></tr>
	<tr><td><a href="http://www.digikey.com/product-detail/en/cui-inc/PJ-002B/CP-002B-ND/96965">DC Barrel Jack, PJ-002B</a></td><td>$0.93</td></tr>
	<tr><td><a href="http://www.digikey.com/product-detail/en/cui-inc/PP3-002B/CP3-1001-ND/992137">DC Barrel Jack Plug, PP3-002B</a></td><td>$1.36</td></tr>
	<tr><td><a href="http://www.digikey.com/product-detail/en/stackpole-electronics-inc/CF14JT2M00/CF14JT2M00CT-ND/1830429">2M Resistor</a></td><td>$0.04</td></tr>
	<tr><td><a href="http://www.digikey.com/product-detail/en/yageo/CFR-25JB-52-2M7/2.7MQBK-ND/687">2.7M Resistor</a></td><td>$0.06</td></tr>
	<tr><td><a href="http://www.digikey.com/product-detail/en/jst-sales-america-inc/B5B-XH-A(LF)(SN)/455-2270-ND/1530483">Motor Plug Socket</a></td><td>$0.21</td></tr>
	<tr><td><a href="http://www.digikey.com/product-detail/en/te-connectivity-alcoswitch-switches/2-1825910-7/450-1642-ND/1632528">Tactile Button</a></td><td>$0.10</td></tr>
	<tr><td><a href="http://www.seeedstudio.com/service/index.php?r=pcb">PCB</a></td><td>$0.99</td></tr>
	<tr><td><a href="https://lowpowerlab.com/shop/moteino-r4">Moteino</a></td><td>$22.95</td></tr>
	<tr><td><a href="http://www.digikey.com/product-detail/en/BH14AAW/BH14AAW-ND/66735">AA Holder</a></td><td>$1.24</td></tr>
	<tr><td><a href="http://www.amazon.com/Energizer-Ultimate-Lithium-Batteries-Original/dp/B01C4PP8FK/ref=sr_1_6?ie=UTF8&qid=1459464495&sr=8-6&keywords=energizer+l91">Energizer Ultimate AA (4)</a></td><td>$6.00</td></tr>
	<tr><td><strong>Total</strong></td><td>$37.95</td></tr>
</table>

In order to talk to the devices from a host computer, you'll also need a Moteino USB ($27). To program the non-USB Moteinos you'll need a FTDI adapter. LowPowerLabs sells one of those for $15, but you may be able to find a better deal elsewhere.

