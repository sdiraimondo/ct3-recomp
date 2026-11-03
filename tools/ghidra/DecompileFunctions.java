// Script GhidraScript (headless) : décompile les fonctions de CT3.exe contenant les adresses listées.
//
// Usage : DecompileFunctions.java <adresses.txt> <sortie.c>
//   adresses.txt : une adresse hexadécimale par ligne (sans 0x), n'importe quelle adresse de la fonction.
//
// Le script ne modifie pas le programme et n'écrit que le fichier de sortie.

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Program;

import java.io.FileWriter;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class DecompileFunctions extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) {
            throw new IllegalArgumentException("usage: <adresses.txt> <sortie.c>");
        }
        Program program = currentProgram;
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(program);
        List<String> lines = Files.readAllLines(Paths.get(args[0]));
        try (PrintWriter w = new PrintWriter(new FileWriter(args[1]))) {
            for (String line : lines) {
                String s = line.trim();
                if (s.isEmpty() || s.startsWith("#")) {
                    continue;
                }
                Address a = program.getAddressFactory().getDefaultAddressSpace().getAddress(s);
                Function f = program.getFunctionManager().getFunctionContaining(a);
                if (f == null) {
                    w.println("// " + s + " : aucune fonction");
                    continue;
                }
                DecompileResults res = decomp.decompileFunction(f, 120, monitor);
                w.println("// ===== " + f.getName() + " @ " + f.getEntryPoint() + " (" + s + ")");
                if (res.decompileCompleted() && res.getDecompiledFunction() != null) {
                    w.println(res.getDecompiledFunction().getC());
                } else {
                    w.println("// échec de décompilation : " + res.getErrorMessage());
                }
            }
        }
        decomp.dispose();
    }
}
