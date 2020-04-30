import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.FileOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.nio.charset.Charset;
import java.lang.Math;
import java.util.TreeMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import net.sourceforge.plantuml.SourceStringReader;
import net.sourceforge.plantuml.FileFormat;
import net.sourceforge.plantuml.FileFormatOption;
import org.apache.batik.transcoder.TranscoderInput;
import org.apache.batik.transcoder.TranscoderOutput;
import org.apache.batik.transcoder.TranscoderException;
import org.apache.batik.transcoder.image.PNGTranscoder;


public class PlotActivity{
    public static void main(String[] args) throws TranscoderException, IOException {
        if(args.length == 3) {
            if(args[0].equals("-v")) {
                new PlotActivity(args[1], args[2], true).process();
            } else {
                System.out.println("java PlotActivity [-v] input output");
            }
        } else if(args.length == 2) {
            new PlotActivity(args[0], args[1], false).process();
        } else {
            System.out.println("Specify input and output filename.");
        }
    }
    
    public PlotActivity(String input, String output, boolean verbose) {
        this.input = input;
        this.output = output;
		this.verbose = verbose;
    }
    
    private String input = null;
    private String output = null;
    private boolean verbose = false;
	private String red_arrow = "-[#red]>";
	private String blue_arrow = "-[#0000FF]>";
    
    public void process() throws TranscoderException, IOException {
        BufferedReader reader = new BufferedReader(new FileReader(input));
        String line = null;
        Graph graph = null;
		float prev_time = (float)0.5;
        int count = 0;
        while((line = reader.readLine()) != null) {
            if(line.matches("^####(.*)####$")) {
                if(graph != null) {
                    String outpath = output+ "/"+graph.name+".png";
					graph.toPng(outpath);
					
                    //String outpath = output+ "/"+graph.name+".html";
					//graph.toHtml(outpath)
					
                }
                graph = new Graph(count++ +"");
                continue;
            }
            String[] tokens = line.split("\\s{4,}");

			// Set line interval
			float timestamp = Float.parseFloat(tokens[2]);
			int interval = 10;
			if (prev_time > 1) {
				interval = Math.round(timestamp - prev_time);
				prev_time = timestamp;
			}

			String op = tokens[1];
			String arrow  = red_arrow;

			// Insert nodes and edge
			if (op.equals("mach_msg_send"))	{
            	graph.addNode(tokens[3]);
            	graph.addNode(tokens[5]);
				// Set arrow color
				float delay = Float.parseFloat(tokens[8]);
				if (delay > 10000)
					arrow = blue_arrow;
            	graph.edges.add(new Edge(interval, timestamp, arrow, tokens[3], tokens[5], tokens[7] + " " + tokens[8]));

			} else if (op.equals("mach_msg_kernel_request")) {
				graph.addNode(tokens[3]);
				// Set arrow color
				float delay = Float.parseFloat(tokens[8]);
				if (delay > 10000)
					arrow = blue_arrow;
            	graph.edges.add(new Edge(interval, timestamp, arrow, tokens[3], tokens[3], tokens[6] + " " + tokens[7] + " "+ tokens[8]));

			} else if (op.equals("mach_msg_recv")) {
				graph.addNode(tokens[3]);
            	graph.edges.add(new Edge(interval, timestamp, null, tokens[5], tokens[3], tokens[7]));
									
			} else if (op.equals("voucher_create")){ 
				graph.addNode(tokens[3]);
				graph.edges.add(new Edge(interval, timestamp, arrow, tokens[3], tokens[5], tokens[7]));

			} else {
				System.err.println("Unknown operation");
			}
        }
        reader.close();
    }

    private class Edge {
		public int interval;
		public float mach_abs_time;
		public String arrow;
        public String from;
        public String to;
        public String label;
        
        public Edge(int interval, float timestamp, String arrow,  String from, String to, String label) {
			this.interval = interval;
			this.mach_abs_time = timestamp;
			this.arrow = arrow;
            this.from = from;
            this.to = to;
            this.label = label;
        }
    }
    
    private class Graph {
        public String name = null;
        
        public Graph(String name) {
            this.name = name;
        }
        
        public List<Edge> edges = new LinkedList<>();
        private Map<String, Integer> pmapping = new TreeMap<>();
        
        public void addNode(String node) {
            if(pmapping.containsKey(node)) {
                return;
            }
            pmapping.put(node, pmapping.size());
        }
		
		private String printNodes(){
			StringBuilder sb = new StringBuilder();
			Iterator<String> labelIt = pmapping.keySet().iterator();
			while(labelIt.hasNext()) {
				String label = labelIt.next();
				sb.append("participant ").append(label).append("\n");
			}
			return sb.toString();
		}
		
		private String printEdges() {
			StringBuilder sb = new StringBuilder();
			for(Edge edge : edges) {
				if (edge.interval < 100) 
					sb.append("||").append(Integer.toString(edge.interval)).append("||\n");
				else
					sb.append("... ").append(Integer.toString(edge.interval)).append("micro seconds later ...\n");	

				if (edge.arrow != null)			
					sb.append(edge.from).append(edge.arrow).append(edge.to).append(":").append(edge.label).append("\n");
				else 
					sb.append("hnote over ").append(edge.to).append(":").append(edge.label).append("\n");
			}
			return sb.toString();
		}

		private String toSvg() throws TranscoderException, IOException {
			StringBuilder source = new StringBuilder();
			source.append("@startuml\n").append(printNodes()).append(printEdges()).append("@enduml\n");
			SourceStringReader ssreader = new SourceStringReader(source.toString());

			final ByteArrayOutputStream os = new ByteArrayOutputStream();
			String desc = ssreader.generateImage(os, new FileFormatOption(FileFormat.SVG));
			os.close();
	
			String svg = new String(os.toByteArray(), Charset.forName("UTF-8"));
			return svg;
		}

        public String toHtml(String filepath) throws TranscoderException, IOException {
			String svg = toSvg();		
			StringBuilder sb = new StringBuilder();
            sb.append("<!doctype html><html><head>\n");
            sb.append("</head><body>\n");
			sb.append(svg);
            sb.append("</body></html>");
			
            BufferedWriter writer = new BufferedWriter(new FileWriter(filepath));
            writer.write(sb.toString());
            writer.close();
            return sb.toString();
        }
		
		public void toPng(String filepath) throws TranscoderException, IOException {
			PNGTranscoder t = new PNGTranscoder();
			byte[] ba = toSvg().getBytes(Charset.forName("UTF-8"));
			TranscoderInput in = new TranscoderInput(new ByteArrayInputStream(ba));
			OutputStream os = new FileOutputStream(filepath);
			TranscoderOutput out = new TranscoderOutput(os);
			t.transcode(in, out);
			os.flush();
			os.close();
		}	
		
    }
        
}
