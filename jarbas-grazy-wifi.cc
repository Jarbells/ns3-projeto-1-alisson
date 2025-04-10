#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("JarbasGrazyWifi");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3); // 0 = Router, 1 = Jarbas, 2 = Grazy

    Names::Add("Router", nodes.Get(0));
    Names::Add("Jarbas", nodes.Get(1));
    Names::Add("Grazy", nodes.Get(2));

    // Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, NodeContainer(nodes.Get(1), nodes.Get(2)));

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, nodes.Get(0));

    // Captura os pacotes para análise (opcional, mas útil)
    phy.EnablePcapAll("jarbas-grazy");

    // Mobilidade
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // Router
    positionAlloc->Add(Vector(1.0, 0.0, 0.0)); // Jarbas (começa 1m à direita)
    positionAlloc->Add(Vector(0.0, 1.0, 0.0)); // Grazy (começa 1m acima)
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes.Get(0));

    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(nodes.Get(1));
    mobility.Install(nodes.Get(2));

    Ptr<WaypointMobilityModel> jarbasMob = nodes.Get(1)->GetObject<WaypointMobilityModel>();
    Ptr<WaypointMobilityModel> grazyMob = nodes.Get(2)->GetObject<WaypointMobilityModel>();

    for (uint32_t i = 0; i <= 25; ++i) {
        double dist = 1.0 + i * (49.0 / 25.0);
        jarbasMob->AddWaypoint(Waypoint(Seconds(i), Vector(dist, 0.0, 0.0)));
        grazyMob->AddWaypoint(Waypoint(Seconds(i), Vector(0.0, dist, 0.0)));
    }
    for (uint32_t i = 26; i <= 50; ++i) {
        double dist = 1.0 + (50 - i) * (49.0 / 25.0);
        jarbasMob->AddWaypoint(Waypoint(Seconds(i), Vector(dist, 0.0, 0.0)));
        grazyMob->AddWaypoint(Waypoint(Seconds(i), Vector(0.0, dist, 0.0)));
    }

    // Pilha IP
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(apDevice, staDevices));

    // Aplicações
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(60.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    clientApps.Add(echoClient.Install(nodes.Get(1)));
    clientApps.Add(echoClient.Install(nodes.Get(2)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(60.0));

    // NetAnim
    AnimationInterface anim("netanim.xml");
    anim.EnablePacketMetadata(true); // Aqui que ativa as setas!
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    anim.UpdateNodeDescription(nodes.Get(0), "Router");
    anim.UpdateNodeDescription(nodes.Get(1), "Jarbas");
    anim.UpdateNodeDescription(nodes.Get(2), "Grazy");
    anim.UpdateNodeColor(nodes.Get(0), 255, 0, 0);
    anim.UpdateNodeColor(nodes.Get(1), 0, 0, 255);
    anim.UpdateNodeColor(nodes.Get(2), 0, 255, 0);

    // Monitoramento
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    monitor->SerializeToXmlFile("flowmon.xml", true, true);
    Simulator::Destroy();

    return 0;
}

