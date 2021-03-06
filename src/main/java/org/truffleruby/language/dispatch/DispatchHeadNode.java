/*
 * Copyright (c) 2013, 2019 Oracle and/or its affiliates. All rights reserved. This
 * code is released under a tri EPL/GPL/LGPL license. You can use it,
 * redistribute it and/or modify it under the terms of the:
 *
 * Eclipse Public License version 2.0, or
 * GNU General Public License version 2, or
 * GNU Lesser General Public License version 2.1.
 */
package org.truffleruby.language.dispatch;

import org.truffleruby.core.proc.RubyProc;
import org.truffleruby.language.RubyContextNode;

import com.oracle.truffle.api.frame.VirtualFrame;

public abstract class DispatchHeadNode extends RubyContextNode {

    protected final boolean ignoreVisibility;
    protected final boolean onlyCallPublic;
    protected final MissingBehavior missingBehavior;
    protected final DispatchAction dispatchAction;

    @Child private DispatchNode first;

    protected DispatchHeadNode(
            boolean ignoreVisibility,
            boolean onlyCallPublic,
            MissingBehavior missingBehavior,
            DispatchAction dispatchAction) {
        this.ignoreVisibility = ignoreVisibility;
        this.onlyCallPublic = onlyCallPublic;
        this.missingBehavior = missingBehavior;
        this.dispatchAction = dispatchAction;
        first = insert(new UnresolvedDispatchNode(ignoreVisibility, onlyCallPublic, missingBehavior, dispatchAction));
    }

    public Object dispatch(
            VirtualFrame frame,
            Object receiverObject,
            Object methodName,
            RubyProc blockObject,
            Object[] argumentsObjects) {
        return first.executeDispatch(
                frame,
                receiverObject,
                methodName,
                blockObject,
                argumentsObjects);
    }

    public void reset(String reason) {
        first.replace(
                new UnresolvedDispatchNode(ignoreVisibility, onlyCallPublic, missingBehavior, dispatchAction),
                reason);
    }

    public DispatchNode getFirstDispatchNode() {
        return first;
    }

    public DispatchAction getDispatchAction() {
        return dispatchAction;
    }

}
