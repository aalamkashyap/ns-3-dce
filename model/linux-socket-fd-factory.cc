#include "linux-socket-fd-factory.h"
#include "include/sim-init.h"
#include "kernel-socket-fd-factory.h"
#include "loader-factory.h"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/simulator.h"
#include "ns3/event-id.h"


NS_LOG_COMPONENT_DEFINE ("LinuxSocketFdFactory");

namespace ns3 {
NS_OBJECT_ENSURE_REGISTERED (LinuxSocketFdFactory);

TypeId
LinuxSocketFdFactory::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LinuxSocketFdFactory")
    .SetParent<KernelSocketFdFactory> ()
    .AddConstructor<LinuxSocketFdFactory> ()
    .AddAttribute ("Library", "File to load in memory",
                   StringValue ("liblinux.so"),
                   MakeStringAccessor (&KernelSocketFdFactory::m_library),
                   MakeStringChecker ())
  ;
  return tid;
}
LinuxSocketFdFactory::LinuxSocketFdFactory ()
{
}

LinuxSocketFdFactory::~LinuxSocketFdFactory ()
{
}

void
LinuxSocketFdFactory::SetTask (std::string path, std::string value)
{
  NS_LOG_FUNCTION (path << value);
  std::vector<std::pair<std::string,struct SimSysFile *> > files = GetSysFileList ();
  for (uint32_t i = 0; i < files.size (); i++)
    {
      if (files[i].first == path)
        {
          const char *s = value.c_str ();
          int toWrite = value.size ();
          int written;
          written = m_exported->sys_file_write (files[i].second, s, toWrite, 0);
          break;
        }
    }
}

void
LinuxSocketFdFactory::Set (std::string path, std::string value)
{
  if (m_manager == 0)
    {
      m_earlySysfs.push_back (std::make_pair (path,value));
    }
  else
    {
      KernelSocketFdFactory::ScheduleTask (MakeEvent (&LinuxSocketFdFactory::SetTask, this, path, value));
    }
}

std::string
LinuxSocketFdFactory::Get (std::string path)
{
  NS_LOG_FUNCTION (path);
  std::string ret;
  std::vector<std::pair<std::string,struct SimSysFile *> > files = GetSysFileList ();
  for (uint32_t i = 0; i < files.size (); i++)
    {
      if (files[i].first == path)
        {
          char buffer[512];
          memset (buffer, 0, sizeof(buffer));
          m_exported->sys_file_read (files[i].second, buffer, sizeof(buffer), 0);
          NS_LOG_FUNCTION ("sysctl read: " << buffer);
          ret = std::string (buffer);
          break;
        }
    }
  return ret;
}

std::vector<std::pair<std::string,struct SimSysFile *> >
LinuxSocketFdFactory::GetSysFileList (void)
{
  struct MyIterator
  {
    struct SimSysIterator head;
    static void ReportStartDir (const struct SimSysIterator *iter, const char *dirname)
    {
      struct MyIterator *self = (struct MyIterator *)iter;
      self->m_stack.push_back (self->m_currentPath);
      self->m_currentPath += "." + std::string (dirname);
    }
    static void ReportEndDir (const struct SimSysIterator *iter)
    {
      struct MyIterator *self = (struct MyIterator *)iter;
      self->m_currentPath = self->m_stack.back ();
      self->m_stack.pop_back ();
    }
    static void ReportFile (const struct SimSysIterator *iter, const char *filename, int flags, struct SimSysFile *file)
    {
      struct MyIterator *self = (struct MyIterator *)iter;
      std::string path = self->m_currentPath + "." + filename;
      self->m_list.push_back (std::make_pair (path, file));
    }
    std::vector<std::string> m_stack;
    std::vector<std::pair<std::string,struct SimSysFile *> > m_list;
    std::string m_currentPath;
  } iter;
  iter.head.report_start_dir = &MyIterator::ReportStartDir;
  iter.head.report_end_dir = &MyIterator::ReportEndDir;
  iter.head.report_file = &MyIterator::ReportFile;
  m_loader->NotifyStartExecute ();
  m_exported->sys_iterate_files ((struct SimSysIterator *)&iter);
  m_loader->NotifyEndExecute ();
  return iter.m_list;
}

void
LinuxSocketFdFactory::InitializeStack (void)
{
  KernelSocketFdFactory::InitializeStack ();
  Set (".net.ipv4.conf.all.forwarding", "1");
  Set (".net.ipv4.conf.all.log_martians", "1");
  Set (".net.ipv6.conf.all.forwarding", "0");

  while (!m_earlySysfs.empty ())
    {
      std::pair<std::string,std::string> op = m_earlySysfs.front ();
      Set (op.first, op.second);
      m_earlySysfs.pop_front ();
    }
}

} // namespace ns3
