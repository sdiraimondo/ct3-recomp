// Script GhidraScript (headless) : exporte les fonctions de CT3.exe en JSON.
//
// Modes (argument 1 du script, arguments 2 et 3 selon le mode) :
//   list <sortie.json>                      : métadonnées de toutes les fonctions
//   pcode <liste_adresses.txt> <sortie.json> : instructions et p-code des fonctions listées
//
// Le script ne modifie pas le programme et n'écrit que le fichier de sortie.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.listing.Program;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.address.AddressRangeIterator;

import java.io.FileWriter;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class ExportFunctions extends GhidraScript {

    private Map<String, String> regNames = new HashMap<>();

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("usage: list <out.json> | pcode <addrs.txt> <out.json>");
        }
        Program program = currentProgram;
        for (Register r : program.getLanguage().getRegisters()) {
            String key = r.getAddress().getAddressSpace().getName() + ":" + r.getOffset() + ":" + (r.getBitLength() / 8);
            regNames.put(key, r.getName());
        }

        if (args[0].equals("list")) {
            exportList(program, args[1]);
        } else if (args[0].equals("pcode")) {
            if (args.length < 3) {
                throw new IllegalArgumentException("pcode requiert un fichier d'adresses et une sortie");
            }
            exportPcode(program, args[1], args[2]);
        } else {
            throw new IllegalArgumentException("mode inconnu : " + args[0]);
        }
    }

    private void exportList(Program program, String out) throws Exception {
        FunctionManager fm = program.getFunctionManager();
        try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
            w.println("[");
            boolean first = true;
            for (Function f : fm.getFunctions(true)) {
                if (f.isExternal()) {
                    continue;
                }
                AddressSetView body = f.getBody();
                long size = body.getNumAddresses();
                int ranges = 0;
                AddressRangeIterator it = body.getAddressRanges();
                StringBuilder rng = new StringBuilder("[");
                while (it.hasNext()) {
                    AddressRange r = it.next();
                    if (ranges > 0) {
                        rng.append(",");
                    }
                    rng.append("[\"").append(r.getMinAddress()).append("\",\"").append(r.getMaxAddress()).append("\"]");
                    ranges++;
                }
                rng.append("]");
                if (!first) {
                    w.println(",");
                }
                first = false;
                w.print("{\"name\":" + q(f.getName()) + ",\"entry\":\"" + f.getEntryPoint() + "\""
                        + ",\"thunk\":" + f.isThunk()
                        + ",\"size\":" + size
                        + ",\"ranges\":" + rng + "}");
            }
            w.println();
            w.println("]");
        }
    }

    private void exportPcode(Program program, String addrFile, String out) throws Exception {
        List<String> lines = Files.readAllLines(Paths.get(addrFile));
        FunctionManager fm = program.getFunctionManager();
        Listing listing = program.getListing();
        try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
            w.println("[");
            boolean firstFn = true;
            for (String line : lines) {
                String trimmed = line.trim();
                if (trimmed.isEmpty() || trimmed.startsWith("#")) {
                    continue;
                }
                String a = trimmed.split("\\s+")[0];
                Address entry = program.getAddressFactory().getAddress(a);
                Function f = fm.getFunctionAt(entry);
                if (f == null) {
                    throw new IllegalStateException("pas de fonction à " + a);
                }
                if (!firstFn) {
                    w.println(",");
                }
                firstFn = false;
                w.println("{\"name\":" + q(f.getName()) + ",\"entry\":\"" + entry + "\",\"insns\":[");
                InstructionIterator ii = listing.getInstructions(f.getBody(), true);
                boolean firstIns = true;
                while (ii.hasNext()) {
                    Instruction ins = ii.next();
                    if (!firstIns) {
                        w.println(",");
                    }
                    firstIns = false;
                    w.print("{\"addr\":\"" + ins.getAddress() + "\",\"len\":" + ins.getLength()
                            + ",\"text\":" + q(ins.toString()) + ",\"pcode\":[");
                    PcodeOp[] ops = ins.getPcode();
                    for (int i = 0; i < ops.length; i++) {
                        PcodeOp op = ops[i];
                        if (i > 0) {
                            w.print(",");
                        }
                        w.print("{\"op\":" + q(op.getMnemonic()) + ",\"out\":" + vn(op.getOutput()) + ",\"in\":[");
                        Varnode[] in = op.getInputs();
                        for (int j = 0; j < in.length; j++) {
                            if (j > 0) {
                                w.print(",");
                            }
                            w.print(vn(in[j]));
                        }
                        w.print("]}");
                    }
                    w.print("]}");
                }
                w.println("]}");
            }
            w.println("]");
        }
    }

    private String vn(Varnode v) {
        if (v == null) {
            return "null";
        }
        String space = v.getAddress().getAddressSpace().getName();
        int size = v.getSize();
        long off = v.getOffset();
        String name = "";
        if (space.equals("register")) {
            name = regNames.getOrDefault(space + ":" + off + ":" + size, "");
        }
        return "{\"s\":\"" + space + "\",\"o\":" + off + ",\"z\":" + size
                + (name.isEmpty() ? "" : ",\"r\":\"" + name + "\"") + "}";
    }

    private static String q(String s) {
        return "\"" + s.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
    }
}
