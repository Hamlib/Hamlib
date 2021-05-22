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
        public class VFO
        {
            public string Name;
            public ulong Freq;
            public string Mode;
            public int Width;
            public bool RX;
            public bool TX;
        }
        public class SpectrumClass
        {
            public int Length;
            public string Data;
            public string Type;
            public int MinLevel;
            public int MaxLevel;
            public int MinStrength;
            public int MaxStrength;
            public double CenterFreq;
            public int Span;
            public double LowFreq;
            public double HighFreq;
        }
        public string ID;
        public List<VFO> VFOs { get; set; }
        public bool Split;
        public bool SatMode;
        public string Rig;
        public string App;
        public string Version;
        public UInt32 Seq;
        public string LastCommand;
        public string CRC;
        public SpectrumClass Spectrum { get; set; }
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

