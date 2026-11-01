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
import ghidra.program.model.symbol.Reference;

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
        } else if (args[0].equals("pcode-all")) {
            exportPcodeAll(program, args[1]);
        } else if (args[0].equals("pcode-all-ptrs")) {
            exportPcodeAllWithPointers(program, args[1]);
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

    /* Fonctions non reconnues par l'analyse automatique mais pointées par des données (tables de
     * fonctions du CRT, etc.) : créées ici avant l'export. Les adresses qui ne se désassemblent pas
     * sont ignorées et comptées. */
    private void exportPcodeAllWithPointers(Program program, String out) throws Exception {
        FunctionManager fm = program.getFunctionManager();
        List<String> addrs = new java.util.ArrayList<>();
        for (Function f : fm.getFunctions(true)) {
            if (!f.isExternal()) {
                addrs.add(f.getEntryPoint().toString());
            }
        }
        ghidra.program.model.mem.Memory mem = program.getMemory();
        java.util.TreeSet<Long> candidates = new java.util.TreeSet<>();
        for (ghidra.program.model.mem.MemoryBlock block : mem.getBlocks()) {
            if (!block.isInitialized() || block.isExecute()) {
                continue;
            }
            for (Address a = block.getStart(); a != null && block.contains(a) && a.compareTo(block.getEnd()) < 0;
                 a = a.add(4)) {
                long v = mem.getInt(a) & 0xFFFFFFFFL;
                Address target = program.getAddressFactory().getDefaultAddressSpace().getAddress(v);
                ghidra.program.model.mem.MemoryBlock tb = mem.getBlock(target);
                if (tb != null && tb.isExecute() && fm.getFunctionAt(target) == null) {
                    candidates.add(v);
                }
            }
        }
        /* Débuts de fonction après un remplissage int3 (convention MSVC : alignement sur 16 octets). Ghidra ne
         * crée pas toujours ces fonctions quand elles ne sont atteintes que par des appels indirects. */
        for (ghidra.program.model.mem.MemoryBlock block : mem.getBlocks()) {
            if (!block.isInitialized() || !block.isExecute()) {
                continue;
            }
            long start = block.getStart().getOffset();
            long end = block.getEnd().getOffset();
            for (long v = (start + 15) & ~15L; v < end; v += 16) {
                Address a = program.getAddressFactory().getDefaultAddressSpace().getAddress(v);
                if (fm.getFunctionAt(a) != null) {
                    continue;
                }
                if (mem.getByte(a.subtract(1)) == (byte) 0xCC && mem.getByte(a) != (byte) 0xCC) {
                    candidates.add(v);
                }
            }
        }
        /* Cibles de branchement ou d'appel direct hors de la fonction courante, sans fonction connue. */
        for (Function f : fm.getFunctions(true)) {
            if (f.isExternal()) {
                continue;
            }
            InstructionIterator bi = program.getListing().getInstructions(f.getBody(), true);
            while (bi.hasNext()) {
                Instruction ins = bi.next();
                /* Adresses de code passées en constante (ex. point d'entrée d'un thread : push imm32). */
                for (Reference r : program.getReferenceManager().getReferencesFrom(ins.getAddress())) {
                    Address ta = r.getToAddress();
                    if (ta != null && ta.isMemoryAddress() && !f.getBody().contains(ta)
                            && fm.getFunctionAt(ta) == null) {
                        ghidra.program.model.mem.MemoryBlock rb = mem.getBlock(ta);
                        if (rb != null && rb.isExecute()) {
                            candidates.add(ta.getOffset());
                        }
                    }
                }
                for (PcodeOp op : ins.getPcode()) {
                    if (op.getOpcode() != PcodeOp.BRANCH && op.getOpcode() != PcodeOp.CBRANCH
                            && op.getOpcode() != PcodeOp.CALL) {
                        continue;
                    }
                    Varnode t = op.getInput(0);
                    if (!t.getAddress().isMemoryAddress()) {
                        continue;
                    }
                    Address target = t.getAddress();
                    if (fm.getFunctionAt(target) == null && op.getOpcode() == PcodeOp.CALL
                            && f.getBody().contains(target)) {
                        /* Appel vers le milieu de la fonction appelante : entrée intérieure. */
                        candidates.add(target.getOffset());
                        continue;
                    }
                    if (f.getBody().contains(target) || fm.getFunctionAt(target) != null) {
                        continue;
                    }
                    ghidra.program.model.mem.MemoryBlock tb = mem.getBlock(target);
                    if (tb != null && tb.isExecute()) {
                        candidates.add(target.getOffset());
                    }
                }
            }
        }
        int created = 0, rejected = 0;
        List<Address> interior = new java.util.ArrayList<>();
        java.util.Set<String> listed = new java.util.HashSet<>(addrs);
        for (long v : candidates) {
            Address target = program.getAddressFactory().getDefaultAddressSpace().getAddress(v);
            if (fm.getFunctionAt(target) != null) {
                /* Créée par Ghidra pendant ce traitement (désassemblage d'un candidat voisin) : à exporter aussi. */
                if (listed.add(target.toString())) {
                    addrs.add(target.toString());
                }
                continue;
            }
            /* Adresse située dans une fonction connue : créer une fonction la découperait. Entrée intérieure. */
            if (fm.getFunctionContaining(target) != null) {
                if (program.getListing().getInstructionAt(target) == null) {
                    disassemble(target);
                }
                if (program.getListing().getInstructionAt(target) != null) {
                    interior.add(target);
                } else {
                    rejected++;
                }
                continue;
            }
            disassemble(target);
            Function f = createFunction(target, null);
            if (f == null) {
                /* Point d'entrée à l'intérieur d'une fonction (ex. gestionnaire SEH) : exporté linéairement. */
                if (program.getListing().getInstructionAt(target) != null) {
                    interior.add(target);
                } else {
                    rejected++;
                    if (rejected <= 40) {
                        Function owner = fm.getFunctionContaining(target);
                        println("rejet " + target + " (fonction englobante : "
                                + (owner == null ? "aucune" : owner.getEntryPoint().toString()) + ")");
                    }
                }
                continue;
            }
            created++;
            addrs.add(target.toString());
        }
        println("pointeurs de code : " + candidates.size() + ", fonctions créées : " + created
                + ", entrées intérieures : " + interior.size() + ", rejetées : " + rejected);
        exportPcodeList(program, addrs, out, interior);
    }

    private void exportPcodeAll(Program program, String out) throws Exception {
        List<String> addrs = new java.util.ArrayList<>();
        for (Function f : program.getFunctionManager().getFunctions(true)) {
            if (!f.isExternal()) {
                addrs.add(f.getEntryPoint().toString());
            }
        }
        exportPcodeList(program, addrs, out);
    }

    private void exportPcode(Program program, String addrFile, String out) throws Exception {
        List<String> addrs = new java.util.ArrayList<>();
        for (String line : Files.readAllLines(Paths.get(addrFile))) {
            String trimmed = line.trim();
            if (trimmed.isEmpty() || trimmed.startsWith("#")) {
                continue;
            }
            addrs.add(trimmed.split("\\s+")[0]);
        }
        exportPcodeList(program, addrs, out);
    }

    private void exportPcodeList(Program program, List<String> addrs, String out) throws Exception {
        exportPcodeList(program, addrs, out, new java.util.ArrayList<>());
    }

    /* Entrées intérieures : instructions linéaires depuis l'adresse, jusqu'au premier RET (inclus). */
    private void exportPcodeList(Program program, List<String> addrs, String out, List<Address> interior)
            throws Exception {
        FunctionManager fm = program.getFunctionManager();
        Listing listing = program.getListing();
        try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
            w.println("[");
            boolean firstFn = true;
            for (String a : addrs) {
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
                writeInstructions(program, w, listing.getInstructions(f.getBody(), true), false);
                w.println("]}");
            }
            for (Address entry : interior) {
                if (!firstFn) {
                    w.println(",");
                }
                firstFn = false;
                w.println("{\"name\":" + q("ENTRY_" + entry) + ",\"entry\":\"" + entry + "\",\"insns\":[");
                writeInstructions(program, w, listing.getInstructions(entry, true), true);
                w.println("]}");
            }
            w.println("]");
        }
    }

    /* Écrit les instructions ; si stopAtReturn, s'arrête après la première instruction portant un RETURN. */
    private void writeInstructions(Program program, PrintWriter w, InstructionIterator ii, boolean stopAtReturn) {
        boolean firstIns = true;
        int count = 0;
        while (ii.hasNext()) {
            Instruction ins = ii.next();
            if (!firstIns) {
                w.println(",");
            }
            firstIns = false;
            StringBuilder targets = new StringBuilder("[");
            boolean firstTarget = true;
            for (Reference r : program.getReferenceManager().getReferencesFrom(ins.getAddress())) {
                if (r.getReferenceType().isComputed() && r.getReferenceType().isJump()) {
                    if (!firstTarget) {
                        targets.append(",");
                    }
                    firstTarget = false;
                    targets.append("\"").append(r.getToAddress()).append("\"");
                }
            }
            targets.append("]");
            w.print("{\"addr\":\"" + ins.getAddress() + "\",\"len\":" + ins.getLength()
                    + ",\"text\":" + q(ins.toString()) + ",\"targets\":" + targets + ",\"pcode\":[");
            PcodeOp[] ops = ins.getPcode();
            boolean hasReturn = false;
            for (int i = 0; i < ops.length; i++) {
                PcodeOp op = ops[i];
                if (i > 0) {
                    w.print(",");
                }
                if (op.getOpcode() == PcodeOp.RETURN) {
                    hasReturn = true;
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
            count++;
            if (stopAtReturn && (hasReturn || count >= 2000)) {
                break;
            }
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
