rule("slint")
    set_extensions(".slint")

    after_load(function(target)
        -- Consumers inline generated C++ bindings. They must not check header
        -- timestamps or compile until generation finishes, otherwise one build
        -- can link the new UI layout with callers compiled against the old one.
        target:set("policy", "build.fence", true)
        local outputdir = path.join(target:autogendir(), "rules", "slint")
        target:add("includedirs", outputdir, {public = true})

        local sourcebatch = target:sourcebatches()["slint"]
        local outputs = {}
        for _, sourcefile in ipairs(sourcebatch and sourcebatch.sourcefiles or {}) do
            local header = path.join(outputdir, path.basename(sourcefile) .. ".h")
            local sources = {}
            -- Slint supports moving definitions out of the header and splitting
            -- them into independently compiled translation units.
            for index = 1, 4 do
                local cppfile = path.join(outputdir,
                    path.basename(sourcefile) .. "_" .. index .. ".cpp")
                table.insert(sources, cppfile)
                target:add("files", cppfile, {always_added = true})
            end
            outputs[sourcefile] = {header = header, sources = sources}
        end
        target:data_set("slint.outputs", outputs)
    end)

    on_config(function(target)
        if target:rule("c++.build") then
            local cpp_rule = target:rule("c++.build"):clone()
            cpp_rule:add("deps", "slint", {order = true})
            target:rule_add(cpp_rule)
        end
    end)

    before_build_file(function(target, sourcefile, opt)
        import("core.project.depend")
        import("core.project.project")
        import("core.tools.gcc.parse_deps", {alias = "parse_deps"})
        import("utils.progress")

        local package = assert(target:pkg("slint"), "the slint package is required by this target")
        local compiler = path.join(package:installdir(), "bin",
            is_host("windows") and "slint-compiler.exe" or "slint-compiler")
        local outputs = target:data("slint.outputs")[sourcefile]
        local depfile = outputs.header .. ".d"
        -- Slint writes the -o path literally into each generated #include.
        -- The custom UI uses fixed light backgrounds. Keep default text and
        -- standard widgets light as well, regardless of the system theme.
        local argv = {sourcefile, "-f", "cpp", "-o", path.absolute(outputs.header),
            "--depfile", depfile, "--style", "fluent-light",
            "--embed-resources=embed-files", "--cpp-namespace", "crossdesk::ui"}
        local missing_output = not os.isfile(outputs.header)
        for _, cppfile in ipairs(outputs.sources) do
            table.insert(argv, "--cpp-file")
            table.insert(argv, path.absolute(cppfile))
            missing_output = missing_output or not os.isfile(cppfile)
        end

        local function dependencies()
            local files = {sourcefile, compiler}
            if os.isfile(depfile) then
                -- Track imported components AND embedded images, including
                -- resources outside the UI directory, using the compiler output.
                table.join2(files, parse_deps(io.readfile(depfile, {continuation = "\\"})))
            end
            files = table.unique(files)
            table.sort(files)
            return files
        end

        local function input_hashes(files)
            -- Rapid edits can share an mtime with the last generation too.
            local hashes = {}
            for _, file in ipairs(files) do
                if file ~= compiler then
                    table.insert(hashes, os.isfile(file) and hash.sha256(file) or "missing")
                end
            end
            return hashes
        end

        local function invalidate_consumers(changed_files)
            for _, consumer in pairs(project.targets()) do
                local uses_target = consumer == target
                for _, dependency in ipairs(consumer:orderdeps()) do
                    uses_target = uses_target or dependency == target
                end
                if uses_target then
                    for _, dependfile in ipairs(consumer:dependfiles()) do
                        local info = depend.load(dependfile, {target = consumer})
                        for _, file in ipairs(info and info.files or {}) do
                            if changed_files[path.absolute(file)] then
                                os.tryrm(dependfile)
                                break
                            end
                        end
                    end
                    -- Archiving/linking uses the same coarse mtime comparison.
                    if consumer:targetfile() then
                        os.tryrm(consumer:dependfile())
                    end
                end
            end
        end

        local dependfile = target:dependfile(outputs.header)
        local previous = depend.load(dependfile) or {}
        local files = dependencies()
        local depopts = {dependfile = dependfile,
            values = {argv, input_hashes(files)}, files = files,
            changed = target:is_rebuilt() or missing_output or not os.isfile(depfile)}
        -- Slint preserves timestamps of unchanged outputs. Use the dependency
        -- cache's successful-generation time, not the oldest output's time.
        depend.on_changed(function()
            progress.show(opt.progress, "${color.build.object}generating.slint %s", sourcefile)
            os.mkdir(path.directory(outputs.header))
            os.vrunv(compiler, argv)
            -- Xmake compares mtimes in whole seconds. A generated header can
            -- change in the same second as a consumer's last compilation, so
            -- ordering alone cannot prevent mixing incompatible UI bindings.
            local generated, changed_files = {}, {}
            for _, file in ipairs(table.join({outputs.header}, outputs.sources)) do
                local absolute = path.absolute(file)
                generated[absolute] = hash.sha256(absolute)
                if generated[absolute] ~= (previous.generated or {})[absolute] then
                    changed_files[absolute] = true
                end
            end
            if not table.empty(changed_files) then
                invalidate_consumers(changed_files)
            end
            -- Save the freshly discovered dependencies on the first build too.
            depopts.files = dependencies()
            depopts.values = {argv, input_hashes(depopts.files)}
            return {generated = generated}
        end, depopts)
    end)
rule_end()
