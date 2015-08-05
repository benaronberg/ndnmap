/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014, Washington University in St. Louis,
 *
 */

#include <boost/asio.hpp>
#include <ndn-cxx/face.hpp>
#include <fstream>
#include <unordered_map>
#include <list>
#include <chrono>
#include "nfdStatusCollector.hpp"
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/scheduler-scoped-event-id.hpp>
#include <ndn-cxx/util/network-monitor.hpp>
#include <sys/wait.h>
#include <time.h>

#define APP_SUFFIX "/ndnmap/stats"
#define SCRIPT_SUFFIX "/script"

int DEBUG = 0;
int LOCAL = 0;
int COLLECTOR = 1;

namespace ndn {
  
class NdnMapServer
{
public:
  NdnMapServer(char* programName)
  : m_programName(programName)
  , m_face(m_io)
  , m_scheduler(m_io)
  , m_terminationSignalSet(m_io)
  {
    m_mapServerAddr = "128.252.153.27";
    m_pollPeriod = 1;
    m_timeoutPeriod = 500;
    
    m_networkMonitor.reset(new util::NetworkMonitor(m_io));
    
  }
  
  void
  usage()
  {
    std::cout << "\n Usage:\n " << m_programName <<
    ""
    "[-h] -f link_file -n number_of_linkids [-k script_file] [-s map_addr] [-t poll_period] [-r timeout_period] [-d debug_mode] [-l store locally] [-p get PIDs]\n"
    " Poll the status of remote clients and update ndnmap website with the status of the links."
    "\n"
    " The clients to pull from are specified in the input file, as well as their requested links "
    "\n"
    "  -h \t\t\t- print this message and exit"
    "\n"
    "  -f file_name \t\t- link_file is name of the file containing pairs associated with linkid."
    "\n"
    " \t\t\t   valid line format: <linkId> <interestPrefix> <LinkIP>"
    "\n"
    " \t\t\t   example: 1 /ndn/edu/arizona 192.168.1.3"
    "\n"
    "  -n number_of_linkids \t- supplied by the linkfile"
    "\n"
    "  -k script_file \t- script_file is name of file containing list of scripts to run."
    "\n"
    "  -s map_addr \t\t- addr added to curl command for ndn map"
    "\n"
    "  -t poll_period \t- in seconds, default is 1 second"
    "\n"
    "  -r timeout_period \t- in milliseconds, default is 500 ms"
    "\n"
    "  -d debug mode \t- 1 set debug on, 0 set debug off (default)"   "\n" 
    "  -l store locally \t- store collected data in log file\n"
    "  -i specify target \t- send an interest to a single node (must be used with -y option\n"
    "  -y specify script \t- run a single script on target node (must be used with -i option\n"
    "  -x suppress Collector requests\n"
    << std::endl;
    exit(1);
  }

  void
  onData(const ndn::Interest& interest, ndn::Data& data, std::string linkPrefix)
  {
    std::cout << "Data received for: " << interest.getName() << std::endl;

    CollectorData reply;   
 
    // check if data is from script request
    ndn::Name cmpName(linkPrefix+SCRIPT_SUFFIX);
    if (cmpName.isPrefixOf(interest.getName())) 
    {
      decodeScriptReply(data);
    }
    else // data is a CollectorData reply 
    {  
      reply.wireDecode(data.getContent().blockFromValue()); 
    
      if(reply.m_statusList.empty())
      {
        std::cerr << "received data is empty!!" << std::endl;
      }
  
      if(LOCAL) 
      {
        std::ofstream logfile;

        logfile.open( "nfdstat_"+getTime()+".log", std::ofstream::app);
        if(logfile.is_open()) 
        {
          //for (unsigned i=0; i< reply.m_statusList.size(); i++)
          //{
            logfile << reply;
            //"---LinkId " << i << std::endl << "FaceID: " << reply.m_statusList[i].getFaceId() << std::endl << "LinkIP: " << reply.m_statusList[i].getLinkIp() << std::endl << "Tx: " << reply.m_statusList[i].getTx() << std::endl << "Rx: " << reply.m_statusList[i].getRx() << std::endl << "Timestamp: " << reply.m_statusList[i].getTimestamp() << std::endl << std::endl;
          //}
        } 
        else if(logfile==NULL) 
        {
          std::cerr << "Error opening logfile for write" << std::endl;
        }
        logfile.close();
      }
    
      // get the list of the remote links requested for this prefix
      std::unordered_map<std::string,std::list<ndn::NdnMapServer::linkPair>>::const_iterator got = m_linksList.find(linkPrefix);
    
      if(got == m_linksList.end())
      {
        std::cerr << "failed to recognize the prefix: " << linkPrefix << std::endl;
      }
      else
      {
        std::list<ndn::NdnMapServer::linkPair> prefixList = got->second;
      
        for (unsigned i=0; i< reply.m_statusList.size(); i++)
        {
          int LinkId = 0;
          std::cout << "Reply: " << reply.m_statusList.at(i);
        
          // get the link id of the current IP
          for (auto pair = prefixList.cbegin(); pair != prefixList.cend(); ++pair)
          {
            if((*pair).linkIp == reply.m_statusList[i].getLinkIp())
            {
              if (DEBUG)
                std::cout << " Link ID for " << linkPrefix << " and " << reply.m_statusList[i].getLinkIp() << " is " << (*pair).linkId << std::endl;
            
              LinkId = (*pair).linkId;
            }
          }
        }
        reply.m_statusList.clear();
      } 
    } 
  } 

  void
  decodeScriptReply(ndn::Data& data)
  { 
     ScriptReply reply;
 
     reply.wireDecode(data.getContent().blockFromValue());
 
     std::string buffer;
     int interestNameSize = data.getName().size();
     buffer += data.getName().get(--interestNameSize).toUri();
     buffer += ": ";
     buffer += reply.getData();

     if(LOCAL)
       storeLocally(buffer); 
     else 
       std::cout << buffer << std::endl;
  }
  
  void
  storeLocally(std::string& buffer)
  {
    std::ofstream logfile;

    logfile.open( "nfdstat_"+getTime()+".log", std::ofstream::app);
    if(logfile.is_open())
    {
      logfile << buffer; 
    }
    else if(logfile==NULL)
    {
      std::cerr << "Error opening logfile for write" << std::endl;
    }
   
    logfile.close();
  }
  
  void
  sendInterests()
  {
    if(DEBUG)
    {
      auto now = std::chrono::system_clock::now();
      auto now_c = std::chrono::system_clock::to_time_t(now);
      std::cout << std::ctime(&now_c) << "about to send interests " <<std::endl;
    }
    
    // send specified interests for each link in 'link_file'
    for(auto it = m_linksList.begin(); it != m_linksList.end(); ++it)
    {
      // request face statuses (unless suppressed with -x option)
      if (COLLECTOR) 
      {
        std::list<linkPair> linkList = it->second;
        ndn::Name name(it->first+APP_SUFFIX);
        
        std::list<linkPair>::iterator itList;
        for (itList=linkList.begin(); itList!=linkList.end(); ++itList)
        {
          ndn::Name::Component dstIp((*itList).linkIp);
          name.append(dstIp);
        }
        ndn::Interest i(name);
        i.setInterestLifetime(ndn::time::milliseconds(m_timeoutPeriod));
        i.setMustBeFresh(true);
        
        m_face.expressInterest(i,
                               bind(&NdnMapServer::onData, this, _1, _2, it->first),
                               bind(&NdnMapServer::onTimeout, this, _1));

        if(DEBUG)
            std::cout << "sent: " << name << std::endl;
      }

      // send any requested script interests
      if(!m_scriptsList.empty())
      {
        for(auto iter = m_scriptsList.begin(); iter != m_scriptsList.end(); ++iter) 
        {
          ndn::Name scripts(it->first+SCRIPT_SUFFIX);
          ndn::Name::Component a_script(*iter);

          if (!a_script.empty())
          {
            scripts.append(a_script);

            ndn::Interest j(scripts);
            j.setInterestLifetime(ndn::time::milliseconds(m_timeoutPeriod));
            j.setMustBeFresh(true);

            m_face.expressInterest(j,
                                bind(&NdnMapServer::onData, this, _1, _2, it->first),
                                bind(&NdnMapServer::onTimeout, this, _1));
            if(DEBUG) std::cout << "SENT: " << scripts << std::endl;
          }
        }
      }
    
    }
    // schedule the next fetch
    m_scheduler.scheduleEvent(time::seconds(m_pollPeriod), bind(&NdnMapServer::sendInterests, this));
  }

  void
  onTimeout(const ndn::Interest& interest)
  {
    std::cout << "onTimeout: " << interest.getName() << std::endl;
  }

  void
  terminate(const boost::system::error_code& error, int signalNo)
  {
    if (error)
    {
      std::cout << "error code = " << error << ", signalNo = " << signalNo << std::endl;
      return;
    }
    m_io.stop();
  }

  void startScheduling()
  {
    // schedule the first event soon
    m_scheduler.scheduleEvent(time::milliseconds(100), bind(&NdnMapServer::sendInterests, this));
  }

  void run()
  {
    m_terminationSignalSet.async_wait(bind(&NdnMapServer::terminate, this, _1, _2));
    
    try
    {
      std::cout << m_programName <<  "polling every " << m_pollPeriod << " seconds" << std::endl;
      std::cout << m_programName <<  "timeout set to  " << m_timeoutPeriod << " milliseconds" << std::endl;
      
      m_io.run();
    }
    catch (std::exception& e)
    {
      std::cerr << "ERROR: " << e.what() << "\n" << std::endl;
      exit(1);
    } 
  }

  void
  parseScriptList(std::string filename)
  {
    std::fstream script_file;
    std::string script_line;

    script_file.open(filename, std::fstream::in);
    if (script_file == NULL)
    {
      std::cerr << "cannot open script file " << filename << std::endl;
      usage();
    }

    while (!script_file.eof()) 
    {
      getline(script_file, script_line); 
      m_scriptsList.push_back(script_line);
    }
  }
  
  void
  addScript(std::string script)
  {
    m_scriptsList.push_back(script);
  }

  void
  addScriptTarget(std::string target)
  {
    if (!m_linksList.empty())
    {
      std::cout << "-i option cannot be used with -f or -k" << std::endl;
      exit(1); 
    }
    // add prefix requested from command line -i option to linksList
    linkPair link_pair;

    link_pair.linkId = 0;
    link_pair.linkIp = target;     

    std::list<linkPair> targetList;
    targetList.push_back(link_pair);
    std::pair<std::string,std::list<linkPair>> pair(target,targetList);
    m_linksList.insert(pair);
  }

  void
  processSingleEvent()
  {
    m_face.processEvents(ndn::time::milliseconds(m_timeoutPeriod));
  }

  std::string
  getTime()
  {
    return m_time;
  }

  void
  setTime()
  {
    time_t now;
    struct tm tstruct;
    char buf[80];

    std::time(&now);
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
    m_time = buf;
    if(DEBUG) std::cout << "TIME: " << m_time << std::endl;
  }

  void
  setMapServerAddr(std::string & addr)
  {
    m_mapServerAddr = addr;
  }

  void
  setPollPeriod(int period)
  {
    m_pollPeriod = period;
  }

  void
  setTimeoutPeriod(int to)
  {
    m_timeoutPeriod = to;
  }
  
  class linkPair
  {
  public:
    int linkId;
    std::string linkIp;
  };
  
  std::unordered_map<std::string,std::list<linkPair>> m_linksList;
  std::list<std::string> m_scriptsList;
  
private:
  boost::asio::io_service m_io;
  ndn::Face m_face;
  util::Scheduler m_scheduler;
  unique_ptr<util::NetworkMonitor> m_networkMonitor;
  boost::asio::signal_set m_terminationSignalSet;
  
  std::string m_programName;
  int m_pollPeriod;
  int m_timeoutPeriod;
  std::string m_mapServerAddr;
  std::string m_time;

};
}

int
main(int argc, char* argv[])
{
  ndn::NdnMapServer ndnmapServer(argv[0]);
  int option;
  std::fstream file;
  int num_lines = 0;
   
  // Parse cmd-line arguments
  while ((option = getopt(argc, argv, "hn:f:s:t:r:d:lk:i:y:x")) != -1)
  {
    switch (option)
    {
      case 'f':
        file.open(optarg, std::fstream::in);
        if (file == NULL)
        {
          std::cout << "cannot open file " << optarg << std::endl;
          ndnmapServer.usage();
        }
        break;
      case 'n':
        num_lines = atoi(optarg);
        break;
      case 'k': 
        ndnmapServer.parseScriptList(optarg);
        break;
      case 's':
        ndnmapServer.setMapServerAddr((std::string&)(optarg));
        break;
      case 't':
        ndnmapServer.setPollPeriod(atoi(optarg));
        break;
      case 'r':
        ndnmapServer.setTimeoutPeriod(atoi(optarg));
        break;
      case 'd':
        DEBUG = atoi(optarg);
        break;
      case 'l':
        LOCAL = 1;
        ndnmapServer.setTime(); //set time for log file name
	      break;
      case 'i':
        ndnmapServer.addScriptTarget(optarg);
        ++num_lines;
        break;
      case 'y':
        ndnmapServer.addScript(optarg);
        ndnmapServer.sendInterests();
        ndnmapServer.processSingleEvent();
        break;
      case 'x':
        COLLECTOR = 0;
        break;
      default:
      case 'h':
        ndnmapServer.usage();
        break;
    }
  }

  if (num_lines < 1)
  {
    ndnmapServer.usage();
    return 1;
  }

  // read link pairs from input file
  if(DEBUG)
    std::cout << "Read from input file" << std::endl;

  for(int i = 0; i < num_lines; ++i)
  {
    ndn::NdnMapServer::linkPair linePair;
    std::string linkPrefix;
    file >> linePair.linkId >> linkPrefix >> linePair.linkIp;
    if(file.fail())
    {
      std::cout << "num_lines was incorrect - too large" << std::endl;
    }
    else
    {
      if(DEBUG)
        std::cout << linkPrefix << ": " << linePair.linkId << ", " << linePair.linkIp << std::endl;

      std::unordered_map<std::string,std::list<ndn::NdnMapServer::linkPair>>::const_iterator got = ndnmapServer.m_linksList.find(linkPrefix);
      
      if(got == ndnmapServer.m_linksList.end())
      {
        std::list<ndn::NdnMapServer::linkPair> prefixList;
        prefixList.push_back(linePair);
        std::pair<std::string,std::list<ndn::NdnMapServer::linkPair>> pair(linkPrefix,prefixList);
        ndnmapServer.m_linksList.insert(pair);
      }
      else
      {
        std::list<ndn::NdnMapServer::linkPair> prefixList = got->second;
        prefixList.push_back(linePair);
        ndnmapServer.m_linksList.at(linkPrefix) = prefixList;
        
      }
    }
  }
  file.close();
   
  ndnmapServer.startScheduling();
  ndnmapServer.run();
  
  std::cout << "exit server..." << std::endl;
  return 0;
}
