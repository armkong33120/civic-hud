from pathlib import Path
p=Path('/Users/arm/Documents/Arduino/civic_hud/WebSettings.h');s=p.read_text().replace('#include <functional>','#include <functional>\n#include <streambuf>\n#include <cstring>')
start=s.index('    // Generates full HTML page');end=s.index('        h << "<!DOCTYPE html>',start)
head=s[start:end]
s=s[:start]+'''    // A reusable fixed-size sink: HTTP and serial share the exact portal renderer.
    class ChunkedHtmlBuffer : public std::streambuf {
    public:
        explicit ChunkedHtmlBuffer(std::function<void(const char*,size_t)> send):m_send(send) {}
        int sync() override { if(m_used){m_send(m_buffer,m_used);m_used=0;}return 0; }
    protected:
        int_type overflow(int_type c) override {
            if(traits_type::eq_int_type(c,traits_type::eof()))return traits_type::not_eof(c);
            m_buffer[m_used++]=traits_type::to_char_type(c);if(m_used==sizeof(m_buffer))sync();return c;
        }
        std::streamsize xsputn(const char* data,std::streamsize count) override {
            std::streamsize remaining=count;
            while(remaining>0){size_t n=std::min(size_t(remaining),sizeof(m_buffer)-m_used);std::memcpy(m_buffer+m_used,data,n);m_used+=n;data+=n;remaining-=n;if(m_used==sizeof(m_buffer))sync();}
            return count;
        }
    private:
        char m_buffer[256];size_t m_used=0;
        std::function<void(const char*,size_t)> m_send;
    };
    static std::string renderHtmlPage(uint32_t currentOdo,const std::string& btName,const std::string& btPin,
                                      const std::vector<MaintenanceItem>& items,const std::vector<ServiceHistoryRecord>& history) {
        std::ostringstream h;renderHtmlTo(h,currentOdo,btName,btPin,items,history);return h.str();
    }
    // Same existing layout, without requiring the board to allocate a full HTML string.
    static void renderHtmlTo(std::ostream& h,uint32_t currentOdo,const std::string& btName,const std::string& btPin,
                             const std::vector<MaintenanceItem>& items,const std::vector<ServiceHistoryRecord>& history) {
'''+s[end:]
s=s.replace('        return h.str();\n    }\n\n    // Handles form','    }\n\n    // Handles form')
start=s.index('            std::string html = renderHtmlPage(',s.index('// GET / : Settings Portal'));end=s.index('\n        });',start)
s=s[:start]+'''            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200,"text/html; charset=utf-8","");
            ChunkedHtmlBuffer buffer([&server](const char* bytes,size_t length){server.sendContent(bytes,length);});
            std::ostream output(&buffer);
            renderHtmlTo(output,odoEngine.getCurrentOdo(),btName,btPin,maintEngine.getItems(),history);
            output.flush();
            server.sendContent("",0);'''+s[end:]
p.write_text(s)
p=Path('/Users/arm/Documents/Arduino/civic_hud/tests/test_m2_regressions.cpp');s=p.read_text();pos=s.index('int main()');s=s[:pos]+'''TEST_TIER3(M2_StreamingPortalExactlyMatchesExistingHtml){MaintenanceEngine m;std::vector<ServiceHistoryRecord>history{{"oil, <special>","",100000,90000,10000,110000}};auto expected=WebSettings::renderHtmlPage(98540,"OBDII","1234",m.getItems(),history);std::string actual;size_t largest=0,chunks=0;WebSettings::ChunkedHtmlBuffer buffer([&](const char*p,size_t n){actual.append(p,n);largest=std::max(largest,n);++chunks;});std::ostream out(&buffer);WebSettings::renderHtmlTo(out,98540,"OBDII","1234",m.getItems(),history);out.flush();ASSERT_EQ(actual,expected);ASSERT_LE(largest,256u);ASSERT_GT(chunks,1u);std::cout<<"      [portal "<<actual.size()<<" bytes, maximum chunk "<<largest<<" bytes, "<<chunks<<" chunks]\\n";}
'''+s[pos:];p.write_text(s)
