import java.io.*;
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
		if (args.length < 2) {
            System.out.println("Specify input and output filename.");
			return;
		}
		File dir = new File(args[1]);
		if (!dir.exists()) {
    		boolean successful = dir.mkdir();
    		if (successful) {
      			System.out.println("directory was created successfully");
    		} else {
      			System.out.println("failed trying to create the directory");
    		}
		}
		if (!dir.isDirectory())
			return;

        if (args.length == 3) {
			System.out.println(args[0] + " " + args[1] + "  " +args[2]);
            if(args[2].equals("-v")) {
                new PlotActivity(args[0], args[1], true).process();
            } else {
                System.out.println("java PlotActivity [-v] input output");
            }
        } else if(args.length == 2) {
			System.out.println(args[0] + " " + args[1]);
            new PlotActivity(args[0], args[1], false).process();
        } 
    }
    
    public PlotActivity(String input, String output, boolean verbose) {
        this.input = input;
        this.output = output;
		this.verbose = verbose;
    }

    private String[] color_array = new String[]{"blue", "aquamarine", "burlywood", "cadetblue", "cornflowerblue",
						"darkgrey" , "darkgreen", "darkorange", "mediumpurple", "gold",
						"forestgreen", "sienna", "yellowgreen", "red"};

    private Map<String, String> tid_proc_mapping = new TreeMap<>();
    private String input = null;
    private String output = null;
    private boolean verbose = false;
    
    public void process() throws TranscoderException, IOException {
        BufferedReader reader = new BufferedReader(new FileReader(input));
        Graph graph = null;
        String line = null;
		float prev_time = (float)0.5;
        long count = 0;
		boolean processing = false;
        while((line = reader.readLine()) != null) {
			if (line.matches("^#Cluster.*")) {
				processing = true;

                if (graph != null) {
                    String outpath = output+ "/"+graph.name+".html";
					graph.toHtml(outpath);
					graph = null;
                }

				if (processing == true) {
					String [] substrs = line.split("[\\s\\_]");
                	graph = new Graph("Cluster_" + substrs[1] + "_part_0");
				}
                continue;
            }

			if (processing == false)
				continue;
			
			count++;		
			if (count % 40000 == 0) {
				String []substrs = graph.name.split("_part_");
				String outpath = output+"/"+graph.name+".html";
				graph.toHtml(outpath);
				int suffix = (int)(count / 40000);
				graph = new Graph(substrs[0] + "_part_" + suffix);
			}

            String[] tokens = line.split("\\s{1,}");
			if (tokens.length == 4) {
				graph.edges.add(new Edge(Float.parseFloat(tokens[0]), Long.parseLong(tokens[1], 16), tokens[2], tokens[3]));
			} else if (tokens.length == 7) {
				graph.edges.add(new Edge(Float.parseFloat(tokens[0]), Long.parseLong(tokens[1], 16), tokens[2], tokens[3],
								Float.parseFloat(tokens[4]), Long.parseLong(tokens[5], 16), tokens[6]));
			} else {
				System.err.println("Unknown operation at " + tokens[0]);
			}
        }

		if (graph != null) {
			String outpath = output+ "/"+graph.name+".html";
			graph.toHtml(outpath);
			graph = null;
		}
        reader.close();
    }

    private class Edge {
		public int interval;
		public float host_abstime;
		public long host_gid;
		public String host_name;
		public String action;
		public float peer_abstime;
		public long peer_gid;
		public String peer_name;
        
        public Edge(float t1, long gid1, String name1, String act, float t2, long gid2, String name2) {
			this.host_abstime = t1;
			this.host_gid = gid1;
			this.host_name = name1;
			this.action = act;
			this.peer_abstime = t2;
			this.peer_gid = gid2;
			this.peer_name = name2;
        }
		
		public Edge(float t1, long gid1, String name1, String act) {
			this.host_abstime = t1;
			this.host_gid = gid1;
			this.host_name = name1;
			this.action = act;
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

		private String printNodesBegin(){
			StringBuilder sb = new StringBuilder();
			Iterator<String> labelIt = pmapping.keySet().iterator();
			while(labelIt.hasNext()) {
				String label = labelIt.next();
				sb.append("activate ").append(label).append("\n");
			}
			return sb.toString();
		}

		private String printNodesEnd(){
			StringBuilder sb = new StringBuilder();
			Iterator<String> labelIt = pmapping.keySet().iterator();
			while(labelIt.hasNext()) {
				String label = labelIt.next();
				sb.append("deactivate ").append(label).append("\n");
			}
			return sb.toString();
		}
		
		private String printEdges() {
			StringBuilder sb = new StringBuilder();
			String boundary_action = "activate";
			//sort edges

			for(Edge edge : edges) {
				addNode(edge.host_name);
				if (pmapping.containsKey(edge.host_name) == false) {
					sb.append("create " + edge.host_name + "\n");
				}		

				if ((edge.action).contains(boundary_action) == true) {
					sb.append(edge.action + " ").append(edge.host_name).append("\n");
				} else {
					//checking if peer node exist
					if (pmapping.containsKey(edge.peer_name) == false)	{
						sb.append("create " + edge.peer_name + "\n");
						addNode(edge.peer_name);
					}
					sb.append(edge.host_name).append(" -> ").append(edge.peer_name).append(": ").append(edge.action).append("\n");
				}
			}
			return sb.toString();
		}

		private String toSvg() throws TranscoderException, IOException {
			String edges = printEdges();
			String nodesbegin = printNodesBegin();
			String nodesend = printNodesEnd();
			
			System.out.println(nodesbegin);
			System.out.println(edges);
			System.out.println(nodesend);
            String sourceStr = "@startuml\n" + printNodesBegin() + printEdges() + printNodesEnd()+ "@enduml\n";
			SourceStringReader ssreader = new SourceStringReader(sourceStr);
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
			sb.append("<script src=\"../dist/svg-pan-zoom.js\"></script>\n");
            sb.append("</head><body>\n");
			sb.append("<div id=\"container\" style=\"width: 800px; height: 500px; border:1px solid black; \">\n");
			sb.append(svg);
			sb.append("\n</div>\n");
    		sb.append("<script>\n");
      		sb.append("window.onload = function() {\n");
        	sb.append("svgPanZoom(\'#demo\', {\n");
          	sb.append("zoomEnabled: true,\n");
          	sb.append("controlIconsEnabled: true,\n");
          	sb.append("fit: true,\n");
          	sb.append("center: true\n");
        	sb.append("});\n");
      		sb.append("};\n");
    		sb.append("</script>\n");
            sb.append("</body></html>");
			
            BufferedWriter writer = new BufferedWriter(new FileWriter(filepath));
            writer.write(sb.toString());
            writer.close();
            return sb.toString();
        }
		
		public void toPng(String filepath) throws TranscoderException, IOException {
			PNGTranscoder png = new PNGTranscoder();
			png.addTranscodingHint(PNGTranscoder.KEY_HEIGHT, 100f);
    		png.addTranscodingHint(PNGTranscoder.KEY_WIDTH, 100f);
			String svg = toSvg();
			
			byte[] ba = svg.getBytes(Charset.forName("UTF-8"));
			TranscoderInput in = new TranscoderInput(new ByteArrayInputStream(ba));
			OutputStream os = new FileOutputStream(filepath);
			TranscoderOutput out = new TranscoderOutput(os);
			png.transcode(in, out);
			os.flush();
			os.close();
		}	
    }
}
