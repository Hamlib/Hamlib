using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using Newtonsoft.Json.Serialization;
using System;
using System.Collections.Generic;
using System.IO;

namespace HamlibMultiCast
{
    public class HamlibMulticastClient
    {
        public string __comment1__;
        public class VFO
        {
            public string Name;
            public ulong Freq;
            public string Mode;
            public int Width;
            public bool RX;
            public bool TX;
            public int WidthLower;
            public int WidthUpper;
        }
        public string __comment_spectrum__;
        public class SpectrumClass
        {
            public string Name;
            public int Length;
            public string __comment_spectrum_data__;
            public string Data;
            public string Type;
            public int MinLevel;
            public int MaxLevel;
            public int MinStrength;
            public int MaxStrength;
            public string __comment_spectrum_center__;
            public double CenterFreq;
            public int Span;
            public string __comment_spectrum_fixed__;
            public double LowFreq;
            public double HighFreq;
        }
        public class LastCommand {
            public string ID;
            public string Command;
            public string Status;
        }
        public string __comment_gps__;
        public class GPS {
            public double Latitude;
            public double Longitude;
            public double Altitude;
        }
        public string ID;
        public string VFOCurr;
        public List<VFO> VFOs { get; set; }
        public bool Split;
        public string __comment_time__;
        public string Time;
        public bool SatMode;
        public string Rig;
        public string App;
        public string __comment_version__;
        public string Version;
        public string __comment_seq__;
        public UInt32 Seq;
        public string __comment_crc__;
        public string CRC;
        public List<SpectrumClass> Spectra { get; set; }
    }
    class Program
    {
        static int Main(string[] args)
        {
            Console.WriteLine("HamlibMultiCast Test Json Deserialize");
            ITraceWriter traceWriter = new MemoryTraceWriter();
            try
            {

                string json = File.ReadAllText("test.json");
                var client = JsonConvert.DeserializeObject<HamlibMulticastClient>(json, new JsonSerializerSettings { TraceWriter = traceWriter, Converters = { new JavaScriptDateTimeConverter() } });
                 Console.WriteLine(traceWriter);
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                return 1;
            }
            return 0;
        }
    }
}

